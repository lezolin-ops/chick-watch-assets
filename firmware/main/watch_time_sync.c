#include "watch_time_sync.h"
#include "watch_clock.h"
#include "watch_wifi.h"
#include "watch_power.h"
#include "esp_netif_sntp.h"
#include "esp_log.h"
#include "freertos/task.h"
#include <time.h>
static TaskHandle_t s_task;
static void worker(void *arg)
{
    (void)arg;
    const char *servers[] = {"ntp.aliyun.com", "pool.ntp.org"};
    watch_power_client_t network = NULL;
    for(;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        watch_wifi_status_t status; watch_wifi_status(&status);
        if(!status.enabled || !status.ip[0]) continue;
        /* PM is initialized after settings starts this task; create lazily.
         * Baseline/failed PM initialization retains the old network policy. */
        if(!network && WATCH_POWER_ENABLED)
            watch_power_client_create("time_sync", WATCH_POWER_NETWORK, &network);
        bool held = network && watch_power_client_acquire(network) == ESP_OK;
        if(network && !held) continue;
        bool synced = false;
        /* One bounded cycle per acquired IP; no permanent hourly polling. */
        for(unsigned i=0; i<2 && !synced; i++) {
            esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(servers[i]);
            esp_err_t err = esp_netif_sntp_init(&config);
            if(err != ESP_OK) { ESP_LOGW("watch_time", "SNTP init: %s", esp_err_to_name(err)); break; }
            for(unsigned second=0;second<15;second++) {
                err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(1000));
                if(err == ESP_OK) { synced=true; break; }
                watch_wifi_status(&status); if(!status.enabled || !status.ip[0]) break;
            }
            esp_netif_sntp_deinit();
            watch_wifi_status(&status); if(!status.enabled || !status.ip[0]) break;
        }
        if(synced) {
            watch_clock_network_time_ready();
            time_t now=time(NULL);struct tm t;localtime_r(&now,&t);
            ESP_LOGI("watch_time", "SNTP synced: %04d-%02d-%02d %02d:%02d:%02d CST",t.tm_year+1900,t.tm_mon+1,t.tm_mday,t.tm_hour,t.tm_min,t.tm_sec);
        } else ESP_LOGW("watch_time", "Time sync unavailable; retaining current clock");
        if(held) watch_power_client_release(network);
    }
}
esp_err_t watch_time_sync_init(void)
{
    return xTaskCreate(worker,"watch_time",6144,NULL,3,&s_task)==pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
void watch_time_sync_network_ready(void) { if(s_task) xTaskNotifyGive(s_task); }
