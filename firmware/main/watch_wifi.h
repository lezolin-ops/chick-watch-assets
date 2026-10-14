#pragma once
#include "esp_err.h"
#include "esp_wifi.h"
#include <stdbool.h>
#define WATCH_WIFI_MAX_APS 20
typedef enum { WATCH_WIFI_IDLE, WATCH_WIFI_SCANNING, WATCH_WIFI_CONNECTING,
               WATCH_WIFI_CONNECTED, WATCH_WIFI_FAILED } watch_wifi_state_t;
typedef struct {
    watch_wifi_state_t state;
    wifi_ap_record_t aps[WATCH_WIFI_MAX_APS];
    unsigned count, revision;
    char ssid[33], ip[16], message[96];
    bool saved, enabled, offline_auto_off, auto_stopped;
    uint32_t connect_attempts;
} watch_wifi_status_t;
esp_err_t watch_wifi_init(void);
esp_err_t watch_wifi_scan(void);
esp_err_t watch_wifi_connect(const wifi_ap_record_t *ap, const char *password);
void watch_wifi_status(watch_wifi_status_t *out);
esp_err_t watch_wifi_set_enabled(bool enabled);

esp_err_t watch_wifi_set_offline_auto_off(bool enabled);
