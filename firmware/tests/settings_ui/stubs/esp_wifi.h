#pragma once
#include <stdint.h>
enum { WIFI_AUTH_OPEN, WIFI_AUTH_WEP, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WPA2_PSK,
    WIFI_AUTH_WPA_WPA2_PSK, WIFI_AUTH_WPA2_ENTERPRISE, WIFI_AUTH_WPA3_PSK, WIFI_AUTH_WPA2_WPA3_PSK };
typedef struct { uint8_t ssid[33]; int authmode; int rssi; } wifi_ap_record_t;
