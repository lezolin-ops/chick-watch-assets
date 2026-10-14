/* ESP-IDF station/scan examples, serialized in a worker; never call LVGL here. */
#include "watch_wifi.h"
#include "watch_power.h"
#include "watch_wifi_policy.h"
#include "esp_timer.h"
#include "watch_time_sync.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include <stdio.h>
#include <string.h>
#define GOT_IP BIT0
#define DISCONNECTED BIT1
#define CANCEL BIT2
typedef struct { bool scan, power, enabled; wifi_config_t config; } command_t;
static QueueHandle_t s_commands;
static SemaphoreHandle_t s_lock;
static EventGroupHandle_t s_events;
static watch_wifi_status_t s_status;
static wifi_config_t s_saved;
static bool s_has_saved;
static unsigned s_reason;
static int64_t s_offline_since=-1;
static watch_wifi_retry_t s_retry;

static bool wifi_enabled(void)
{
    if(!s_lock) return false;
    xSemaphoreTake(s_lock,portMAX_DELAY); bool enabled=s_status.enabled; xSemaphoreGive(s_lock);
    return enabled;
}

static void state(watch_wifi_state_t value, const char *message)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.state = value;
    snprintf(s_status.message, sizeof(s_status.message), "%s", message);
    s_status.revision++;
    xSemaphoreGive(s_lock);
}
static void event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if(base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ip = data;
        xSemaphoreTake(s_lock, portMAX_DELAY);
        bool enabled = s_status.enabled;
        if(!enabled) { xSemaphoreGive(s_lock); return; }
        snprintf(s_status.ip, sizeof(s_status.ip), IPSTR, IP2STR(&ip->ip_info.ip));
        xSemaphoreGive(s_lock);
        xEventGroupSetBits(s_events, GOT_IP);
        ESP_LOGI("watch_wifi", "Connected; acquired IP " IPSTR, IP2STR(&ip->ip_info.ip));
        if(enabled) watch_time_sync_network_ready();
    } else if(base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_reason = ((wifi_event_sta_disconnected_t *)data)->reason;
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_status.ip[0] = 0;
        if(s_status.state == WATCH_WIFI_CONNECTED) {
            s_status.state = WATCH_WIFI_FAILED;
            snprintf(s_status.message, sizeof(s_status.message), "连接已断开，稍后重试");
            s_status.revision++;
        }
        xSemaphoreGive(s_lock);
        xEventGroupClearBits(s_events, GOT_IP);
        xEventGroupSetBits(s_events, DISCONNECTED);
    }
}
static bool save_config(const wifi_config_t *config)
{
    nvs_handle_t nvs;
    if(nvs_open("pet_network", NVS_READWRITE, &nvs) != ESP_OK) return false;
    esp_err_t err = nvs_set_blob(nvs, "station", config, sizeof(*config));
    if(err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err == ESP_OK;
}
static void connect_network(const wifi_config_t *config)
{
    if(!wifi_enabled()) return;
    state(WATCH_WIFI_CONNECTING, "正在连接…");
    xSemaphoreTake(s_lock, portMAX_DELAY);
    memcpy(s_status.ssid, config->sta.ssid, 32); s_status.ssid[32] = 0;
    s_status.saved = false; s_status.ip[0] = 0;
    xSemaphoreGive(s_lock);
    xEventGroupClearBits(s_events, GOT_IP | DISCONNECTED | CANCEL);
    if(esp_wifi_disconnect() == ESP_OK)
        xEventGroupWaitBits(s_events, DISCONNECTED, pdTRUE, pdFALSE, pdMS_TO_TICKS(2000));
    xEventGroupClearBits(s_events, GOT_IP | DISCONNECTED);
    wifi_config_t mutable_config = *config;
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &mutable_config);
    memset(&mutable_config, 0, sizeof(mutable_config));
    if(err != ESP_OK) { state(WATCH_WIFI_FAILED, "网络配置无效"); return; }
    bool success = false;
    for(int attempt = 0; attempt < 2; attempt++) {
        xEventGroupClearBits(s_events, GOT_IP | DISCONNECTED);
        xSemaphoreTake(s_lock,portMAX_DELAY);s_status.connect_attempts++;xSemaphoreGive(s_lock);
        err = esp_wifi_connect();
        if(err != ESP_OK) break;
        EventBits_t bits = xEventGroupWaitBits(s_events, GOT_IP | DISCONNECTED | CANCEL,
            pdTRUE, pdFALSE, pdMS_TO_TICKS(15000));
        if((bits & CANCEL) || !wifi_enabled()) { esp_wifi_disconnect(); return; }
        if(bits & GOT_IP) { success = true; break; }
        if(!(bits & DISCONNECTED)) {
            esp_wifi_disconnect();
            xEventGroupWaitBits(s_events, DISCONNECTED, pdTRUE, pdFALSE, pdMS_TO_TICKS(2000));
        }
    }
    if(success) {
        s_offline_since=-1;
        watch_wifi_retry_reset(&s_retry);
        bool saved = save_config(config);
        s_saved = *config; s_has_saved = true;
        xSemaphoreTake(s_lock, portMAX_DELAY); s_status.saved = saved; xSemaphoreGive(s_lock);
        state(WATCH_WIFI_CONNECTED, saved ? "下次开机会自动连接" : "已连接，但保存失败");
        ESP_LOGI("watch_wifi", "Connection ready; credentials %s", saved ? "saved" : "not saved");
    } else {
        esp_wifi_disconnect();
        const char *msg = s_reason == WIFI_REASON_NO_AP_FOUND ? "未找到网络，请靠近路由器" :
            s_reason == WIFI_REASON_AUTH_FAIL || s_reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
            s_reason == WIFI_REASON_HANDSHAKE_TIMEOUT ? "认证失败，请检查密码" : "连接超时，请重试";
        state(WATCH_WIFI_FAILED, msg);
        unsigned delay=watch_wifi_retry_failed(&s_retry,esp_timer_get_time());
        ESP_LOGW("watch_wifi", "Connection failed; reason=%u", s_reason);
        ESP_LOGI("watch_wifi", "Reconnect backoff: next round in %us (stage=%u)",delay,s_retry.stage);
    }
}
static void scan_networks(void)
{
    state(WATCH_WIFI_SCANNING, "正在搜索 Wi-Fi…");
    wifi_scan_config_t config = {0};
    esp_err_t err = esp_wifi_scan_start(&config, true);
    wifi_ap_record_t records[WATCH_WIFI_MAX_APS];
    uint16_t count = WATCH_WIFI_MAX_APS;
    if(err == ESP_OK) err = esp_wifi_scan_get_ap_records(&count, records);
    if(err != ESP_OK) { esp_wifi_clear_ap_list(); state(WATCH_WIFI_FAILED, "搜索失败，请重试"); return; }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.count = 0;
    for(unsigned i = 0; i < count; i++) {
        if(!records[i].ssid[0]) continue;
        bool duplicate = false;
        for(unsigned j = 0; j < s_status.count; j++)
            if(!memcmp(s_status.aps[j].ssid, records[i].ssid, 32)) duplicate = true;
        if(!duplicate) s_status.aps[s_status.count++] = records[i];
    }
    bool connected = s_status.ip[0] != 0;
    xSemaphoreGive(s_lock);
    state(connected ? WATCH_WIFI_CONNECTED : WATCH_WIFI_IDLE, "仅支持 2.4 GHz 网络");
}
static void worker(void *arg)
{
    (void)arg;
    if(s_has_saved) connect_network(&s_saved);
    for(;;) {
        command_t command = {0};
        if(xQueueReceive(s_commands, &command, pdMS_TO_TICKS(30000))) {
            if(command.power) {
                s_offline_since=-1;
                watch_wifi_retry_reset(&s_retry);
                if(!command.enabled) {
                    esp_wifi_disconnect();
                    esp_err_t err = esp_wifi_stop();
                    if(err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED) {
                        xSemaphoreTake(s_lock, portMAX_DELAY); s_status.enabled = true; xSemaphoreGive(s_lock);
                        state(WATCH_WIFI_FAILED, "Wi-Fi 关闭失败");
                        ESP_LOGW("watch_wifi", "Radio stop failed: %s", esp_err_to_name(err));
                        continue;
                    }
                    xSemaphoreTake(s_lock, portMAX_DELAY); s_status.ip[0]=0; s_status.count=0; xSemaphoreGive(s_lock);
                    state(WATCH_WIFI_IDLE, "Wi-Fi 已关闭");
                    ESP_LOGI("watch_wifi", "Radio stopped; saved credentials retained");
                } else {
                    esp_err_t err=esp_wifi_start();
                    if(err!=ESP_OK) state(WATCH_WIFI_FAILED,"Wi-Fi 开启失败");
                    else if(s_has_saved) connect_network(&s_saved);
                    else scan_networks();
                }
            } else if(command.scan) scan_networks(); else {
                watch_wifi_retry_reset(&s_retry);
                connect_network(&command.config);
            }
            memset(&command, 0, sizeof(command));
        } else {
            xSemaphoreTake(s_lock,portMAX_DELAY);
            bool retry=s_has_saved && s_status.enabled && !s_status.ip[0] && (s_status.state==WATCH_WIFI_FAILED||s_status.state==WATCH_WIFI_IDLE);
            xSemaphoreGive(s_lock);
            watch_wifi_status_t snapshot;watch_wifi_status(&snapshot);
            int64_t now=esp_timer_get_time();
            if(!snapshot.enabled||snapshot.ip[0]){s_offline_since=-1;watch_wifi_retry_reset(&s_retry);}
            else if(s_offline_since<0)s_offline_since=now;
            if(watch_wifi_offline_due(snapshot.enabled,snapshot.offline_auto_off,snapshot.ip[0]!=0,s_offline_since,now)) {
                if(watch_power_try_stop_offline_network()==ESP_OK) {
                    xSemaphoreTake(s_lock,portMAX_DELAY);s_status.auto_stopped=true;s_status.revision++;xSemaphoreGive(s_lock);
                    ESP_LOGI("watch_wifi","Offline timeout; auto-stop queued; manual enable available");
                    continue;
                }
            }
            if(retry&&watch_wifi_retry_due(&s_retry,now)) connect_network(&s_saved);
        }
    }
}
esp_err_t watch_wifi_init(void)
{
    s_lock = xSemaphoreCreateMutex(); s_events = xEventGroupCreate();
    s_commands = xQueueCreate(1, sizeof(command_t));
    if(!s_lock || !s_events || !s_commands) return ESP_ERR_NO_MEM;
    esp_err_t err = esp_netif_init(); if(err != ESP_OK) return err;
    err = esp_event_loop_create_default();
    if(err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    if(!esp_netif_create_default_wifi_sta()) return ESP_ERR_NO_MEM;
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&config); if(err != ESP_OK) return err;
    err = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, event, NULL);
    if(err != ESP_OK) return err;
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event, NULL);
    if(err != ESP_OK) return err;
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM); if(err != ESP_OK) return err;
    err = esp_wifi_set_mode(WIFI_MODE_STA); if(err != ESP_OK) return err;
    err = watch_time_sync_init(); if(err != ESP_OK) return err;
    err = esp_wifi_start(); if(err != ESP_OK) return err;
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    nvs_handle_t nvs;
    if(nvs_open("pet_network", NVS_READONLY, &nvs) == ESP_OK) {
        uint8_t auto_off=0;
        if(nvs_get_u8(nvs,"offline_off",&auto_off)==ESP_OK&&auto_off==1)s_status.offline_auto_off=true;
        size_t size = sizeof(s_saved);
        s_has_saved = nvs_get_blob(nvs, "station", &s_saved, &size) == ESP_OK &&
            size == sizeof(s_saved) && s_saved.sta.ssid[0];
        nvs_close(nvs);
    }
    xSemaphoreTake(s_lock, portMAX_DELAY); s_status.enabled=true; xSemaphoreGive(s_lock);
    state(WATCH_WIFI_IDLE, "尚未连接 Wi-Fi");
    return xTaskCreate(worker, "watch_wifi", 8192, NULL, 4, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
esp_err_t watch_wifi_scan(void)
{
    if(!wifi_enabled()) return ESP_ERR_INVALID_STATE;
    command_t command = {.scan = true};
    return s_commands && xQueueSend(s_commands, &command, 0) ? ESP_OK : ESP_ERR_INVALID_STATE;
}
esp_err_t watch_wifi_connect(const wifi_ap_record_t *ap, const char *password)
{
    if(!wifi_enabled()) return ESP_ERR_INVALID_STATE;
    if(!ap || !password || strlen(password) > 63) return ESP_ERR_INVALID_ARG;
    command_t command = {0};
    memcpy(command.config.sta.ssid, ap->ssid, 32);
    memcpy(command.config.sta.password, password, strlen(password));
    command.config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    command.config.sta.threshold.authmode = ap->authmode == WIFI_AUTH_OPEN ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    command.config.sta.pmf_cfg.capable = true;
    command.config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    bool queued = s_commands && xQueueSend(s_commands, &command, 0);
    memset(&command, 0, sizeof(command));
    return queued ? ESP_OK : ESP_ERR_INVALID_STATE;
}
void watch_wifi_status(watch_wifi_status_t *out)
{
    if(!s_lock) { memset(out, 0, sizeof(*out)); return; }
    xSemaphoreTake(s_lock, portMAX_DELAY); *out = s_status; xSemaphoreGive(s_lock);
}
esp_err_t watch_wifi_set_enabled(bool enabled)
{
    command_t command={.power=true,.enabled=enabled};
    if(!s_commands) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_lock,portMAX_DELAY);
    if(!xQueueSend(s_commands,&command,0)) { xSemaphoreGive(s_lock); return ESP_ERR_INVALID_STATE; }
    s_status.enabled=enabled;
    if(enabled)s_status.auto_stopped=false;
    if(!enabled) s_status.ip[0]=0;
    s_status.revision++;
    xSemaphoreGive(s_lock);
    if(!enabled) xEventGroupSetBits(s_events,CANCEL);
    return ESP_OK;
}

esp_err_t watch_wifi_set_offline_auto_off(bool enabled)
{
    if(!s_lock)return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_lock,portMAX_DELAY);
    nvs_handle_t nvs;esp_err_t err=nvs_open("pet_network",NVS_READWRITE,&nvs);
    if(err==ESP_OK){err=nvs_set_u8(nvs,"offline_off",enabled);if(err==ESP_OK)err=nvs_commit(nvs);nvs_close(nvs);}
    if(err==ESP_OK){s_status.offline_auto_off=enabled;s_status.revision++;}
    xSemaphoreGive(s_lock);return err;
}
