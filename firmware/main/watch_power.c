#include "watch_power.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "freertos/semphr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "nvs.h"
#include "watch_wifi.h"

struct watch_power_client {
    struct watch_power_client *next;
    esp_pm_lock_handle_t cpu, awake;
    unsigned needs;
    bool held;
    int64_t held_since_us;
    char name[32];
};
static SemaphoreHandle_t s_clients_lock;
static struct watch_power_client *s_clients;
static unsigned s_network_users;
static bool s_sleep_wifi_off;

static esp_pm_lock_handle_t s_ui_lock;
static bool s_locked;
static uint32_t s_tick_offset;
static const char *TAG = "watch_power";
#if CONFIG_PM_LIGHT_SLEEP_CALLBACKS
static esp_err_t sleep_exit(int64_t us,void *arg);
#endif

static uint32_t monotonic_tick(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000) + s_tick_offset;
}

void watch_power_set_blanked(bool blanked)
{
    bool want_locked = !blanked;
    if(!s_ui_lock || want_locked == s_locked) return;
    esp_err_t err = blanked ? esp_pm_lock_release(s_ui_lock) : esp_pm_lock_acquire(s_ui_lock);
    if(err == ESP_OK) {
        s_locked = !blanked;
        ESP_LOGI(TAG, "UI %s; automatic light sleep %s when drivers idle",
                 blanked ? "blanked" : "active", blanked ? "eligible" : "blocked");
    } else ESP_LOGW(TAG, "UI power lock: %s", esp_err_to_name(err));
}

esp_err_t watch_power_init(void)
{
    if(!WATCH_POWER_ENABLED) {
        ESP_LOGI(TAG, "Baseline comparison: fixed CPU, periodic LVGL tick, original Wi-Fi policy");
        return ESP_OK;
    }
    s_clients_lock = xSemaphoreCreateMutex();
    if(!s_clients_lock) return ESP_ERR_NO_MEM;
    nvs_handle_t nvs;
    if(nvs_open("pet_power", NVS_READONLY, &nvs) == ESP_OK) {
        uint8_t off = 0;
        if(nvs_get_u8(nvs, "sleep_wifi_off", &off) == ESP_OK && off <= 1) s_sleep_wifi_off = off;
        nvs_close(nvs);
    }
    /* GPIO10 is SYS_OUT/PWR in the official schematic; do not use its
     * unrelated EXIO5/AXP_IRQ label or an octal-PSRAM pin as a wake input. */
    gpio_config_t key = {.pin_bit_mask = 1ULL << GPIO_NUM_10, .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE, .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    esp_err_t err = gpio_config(&key);
    if(err != ESP_OK) return err;
    err = gpio_wakeup_enable(GPIO_NUM_10, GPIO_INTR_HIGH_LEVEL);
    if(err != ESP_OK) return err;
    err = gpio_wakeup_enable(GPIO_NUM_0, GPIO_INTR_LOW_LEVEL);
    if(err != ESP_OK) return err;
    err = esp_sleep_enable_gpio_wakeup();
    if(err != ESP_OK) return err;
    /* IDF's ESP_SLEEP_GPIO_RESET_WORKAROUND selects PM_SLP_DISABLE_GPIO
     * with tickless idle. Preserve active levels of display/TP resets,
     * QSPI, shared I2C and wake inputs instead of letting them float. */
    const int preserved[] = {0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 38};
    for(unsigned i = 0; i < sizeof(preserved) / sizeof(preserved[0]); i++) {
        err = gpio_sleep_sel_dis(preserved[i]);
        if(err != ESP_OK) return err;
    }

    err = esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "watch_ui", &s_ui_lock);
    if(err != ESP_OK) return err;
    err = esp_pm_lock_acquire(s_ui_lock);
    if(err != ESP_OK) return err;
    s_locked = true;
    const esp_pm_config_t pm = {.max_freq_mhz = WATCH_CPU_MAX_MHZ,
        .min_freq_mhz = WATCH_OPT_DFS ? WATCH_CPU_MIN_MHZ : WATCH_CPU_MAX_MHZ,
        .light_sleep_enable = WATCH_OPT_SLEEP};
    err = esp_pm_configure(&pm);
    if(err != ESP_OK) return err;

#if CONFIG_PM_LIGHT_SLEEP_CALLBACKS
    esp_pm_sleep_cbs_register_config_t callbacks={.exit_cb=sleep_exit};
    err=esp_pm_light_sleep_register_cbs(&callbacks);
    if(err!=ESP_OK)return err;
#endif
    /* LVGL port's 5ms periodic tick otherwise wakes the CPU even with a
     * static/black screen. Public stop disables its timer and LVGL handling;
     * enable handling again using elapsed real time, including sleep time.
     * This app must not call lvgl_port_resume(), which restarts that timer. */
    if(WATCH_OPT_TICK) {
        err = lvgl_port_stop();
        if(err != ESP_OK) return err;
        s_tick_offset = lv_tick_get() - (uint32_t)(esp_timer_get_time() / 1000);
        lv_tick_set_cb(monotonic_tick);
        lv_timer_enable(true);
    }
    ESP_LOGI(TAG, "CPU %d-%dMHz; tick=%d scene=%d sleep=%d (black screen only)",
             pm.min_freq_mhz, pm.max_freq_mhz, WATCH_OPT_TICK, WATCH_OPT_SCENE, WATCH_OPT_SLEEP);
    ESP_LOGI(TAG, "Sleep Wi-Fi policy: %s", s_sleep_wifi_off ? "stop when no app requires network" : "retain connection");
    return ESP_OK;
}

bool watch_power_network_may_stop(void)
{
    if(!WATCH_POWER_ENABLED || !s_clients_lock) return false;
    xSemaphoreTake(s_clients_lock, portMAX_DELAY);
    bool idle = s_sleep_wifi_off && s_network_users == 0;
    xSemaphoreGive(s_clients_lock);
    return idle;
}

static esp_err_t try_stop_network(bool offline)
{
    if(!WATCH_POWER_ENABLED || !s_clients_lock) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_clients_lock, portMAX_DELAY);
    esp_err_t err = ESP_ERR_INVALID_STATE;
    if((offline || s_sleep_wifi_off) && !s_network_users) {
        watch_wifi_status_t status; watch_wifi_status(&status);
        err = status.enabled ? watch_wifi_set_enabled(false) : ESP_OK;
    }
    xSemaphoreGive(s_clients_lock);
    return err;
}

esp_err_t watch_power_try_stop_network(void) { return try_stop_network(false); }
esp_err_t watch_power_try_stop_offline_network(void) { return try_stop_network(true); }

bool watch_power_sleep_wifi_off(void)
{
    if(!s_clients_lock) return false;
    xSemaphoreTake(s_clients_lock, portMAX_DELAY);
    bool off = s_sleep_wifi_off;
    xSemaphoreGive(s_clients_lock);
    return off;
}

esp_err_t watch_power_set_sleep_wifi_off(bool off)
{
    if(!s_clients_lock) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_clients_lock, portMAX_DELAY);
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("pet_power", NVS_READWRITE, &nvs);
    if(err == ESP_OK) {
        err = nvs_set_u8(nvs, "sleep_wifi_off", off);
        if(err == ESP_OK) err = nvs_commit(nvs);
        nvs_close(nvs);
    }
    if(err == ESP_OK) s_sleep_wifi_off = off;
    xSemaphoreGive(s_clients_lock);
    return err;
}

esp_err_t watch_power_client_create(const char *name, unsigned needs, watch_power_client_t *out)
{
    if(!out || !name || !name[0] || strlen(name) >= 32 || !needs || (needs & ~7U)) return ESP_ERR_INVALID_ARG;
    *out = NULL;
    if(!s_clients_lock) return ESP_ERR_INVALID_STATE;
    struct watch_power_client *c = calloc(1, sizeof(*c));
    if(!c) return ESP_ERR_NO_MEM;
    snprintf(c->name, sizeof(c->name), "%s", name); c->needs = needs;
    esp_err_t err = ESP_OK;
    if(needs & WATCH_POWER_CPU) err = esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, c->name, &c->cpu);
    if(err == ESP_OK && (needs & WATCH_POWER_AWAKE))
        err = esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, c->name, &c->awake);
    if(err != ESP_OK) { if(c->cpu) esp_pm_lock_delete(c->cpu); free(c); return err; }
    xSemaphoreTake(s_clients_lock, portMAX_DELAY);
    c->next = s_clients; s_clients = c;
    xSemaphoreGive(s_clients_lock);
    *out = c; return ESP_OK;
}

esp_err_t watch_power_client_acquire(watch_power_client_t c)
{
    if(!c || !s_clients_lock) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_clients_lock, portMAX_DELAY);
    esp_err_t err = c->held ? ESP_ERR_INVALID_STATE : ESP_OK;
    if(err == ESP_OK && (c->needs & WATCH_POWER_NETWORK)) {
        watch_wifi_status_t status; watch_wifi_status(&status);
        if(!status.enabled) err = ESP_ERR_INVALID_STATE;
    }
    if(err == ESP_OK && c->cpu) err = esp_pm_lock_acquire(c->cpu);
    if(err == ESP_OK && c->awake) {
        err = esp_pm_lock_acquire(c->awake);
        if(err != ESP_OK && c->cpu) esp_pm_lock_release(c->cpu);
    }
    if(err == ESP_OK) {
        c->held = true;
        c->held_since_us = esp_timer_get_time();
        if(c->needs & WATCH_POWER_NETWORK) s_network_users++;
    }
    xSemaphoreGive(s_clients_lock);
    return err;
}

esp_err_t watch_power_client_release(watch_power_client_t c)
{
    if(!c || !s_clients_lock) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_clients_lock, portMAX_DELAY);
    if(!c->held) { xSemaphoreGive(s_clients_lock); return ESP_ERR_INVALID_STATE; }
    if(c->awake) esp_pm_lock_release(c->awake);
    if(c->cpu) esp_pm_lock_release(c->cpu);
    if(c->needs & WATCH_POWER_NETWORK) s_network_users--;
    c->held = false;
    xSemaphoreGive(s_clients_lock);
    return ESP_OK;
}

esp_err_t watch_power_client_delete(watch_power_client_t c)
{
    if(!c || !s_clients_lock) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_clients_lock, portMAX_DELAY);
    if(c->held) { xSemaphoreGive(s_clients_lock); return ESP_ERR_INVALID_STATE; }
    struct watch_power_client **link = &s_clients;
    while(*link && *link != c) link = &(*link)->next;
    if(!*link) { xSemaphoreGive(s_clients_lock); return ESP_ERR_INVALID_ARG; }
    *link = c->next;
    if(c->cpu) esp_pm_lock_delete(c->cpu);
    if(c->awake) esp_pm_lock_delete(c->awake);
    xSemaphoreGive(s_clients_lock);
    free(c); return ESP_OK;
}

void watch_power_dump_clients(void)
{
    if(!s_clients_lock) return;
    xSemaphoreTake(s_clients_lock, portMAX_DELAY);
    for(struct watch_power_client *c = s_clients; c; c = c->next)
        ESP_LOGI(TAG, "client=%s needs=%u held=%u held_ms=%lld", c->name, c->needs, c->held,
                 c->held ? (long long)((esp_timer_get_time() - c->held_since_us) / 1000) : 0LL);
    xSemaphoreGive(s_clients_lock);
}

void watch_power_client_counts(uint8_t *cpu,uint8_t *awake,uint8_t *network)
{
    unsigned a=0,b=0,c=0;
    if(s_clients_lock) {
        xSemaphoreTake(s_clients_lock,portMAX_DELAY);
        for(struct watch_power_client *p=s_clients;p;p=p->next) if(p->held) {
            if(p->needs&WATCH_POWER_CPU) a++;
            if(p->needs&WATCH_POWER_AWAKE) b++;
            if(p->needs&WATCH_POWER_NETWORK) c++;
        }
        xSemaphoreGive(s_clients_lock);
    }
    *cpu=a>255?255:a;*awake=b>255?255:b;*network=c>255?255:c;
}

#if CONFIG_PM_LIGHT_SLEEP_CALLBACKS
/* Called from idle context: only short spinlock-protected accounting. */
static portMUX_TYPE s_sleep_mux=portMUX_INITIALIZER_UNLOCKED;
static uint64_t s_slept_us;
static esp_err_t sleep_exit(int64_t us,void *arg) {
    (void)arg;
    if(us>0){portENTER_CRITICAL(&s_sleep_mux);s_slept_us+=(uint64_t)us;portEXIT_CRITICAL(&s_sleep_mux);}
    return ESP_OK;
}
#endif
void watch_power_sleep_stats(uint64_t *elapsed_us,uint64_t *slept_us) {
    *elapsed_us=(uint64_t)esp_timer_get_time();*slept_us=0;
#if CONFIG_PM_LIGHT_SLEEP_CALLBACKS
    portENTER_CRITICAL(&s_sleep_mux);*slept_us=s_slept_us;portEXIT_CRITICAL(&s_sleep_mux);
#endif
}
