/* First AOD prototype: RTC clock, sparse black UI, touch wake, idle return. */
#include "watch_clock.h"
#include "pet.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

static lv_obj_t *s_pet_screen, *s_clock_screen, *s_black_screen, *s_time, *s_date, *s_power;
static bool s_showing;
static bool s_blanked;
static uint32_t s_clock_entered;
static int s_last_minute = -1;
static i2c_master_dev_handle_t s_rtc;
static i2c_master_dev_handle_t s_pmu;
static uint32_t s_power_checked;
static bool s_power_has_checked;
static char s_power_text[64];
static const char *TAG = "watch_clock";
static int from_bcd(uint8_t n) { return (n >> 4) * 10 + (n & 15); }
static uint8_t to_bcd(int n) { return ((n / 10) << 4) | (n % 10); }

/* Read-only AXP2101 monitoring; leave charging/voltage settings untouched.
 * Register meanings follow the official demo's bundled XPowersLib. */
static void update_power(bool force)
{
    uint32_t now = lv_tick_get();
    if(!force && s_power_has_checked && now - s_power_checked < 5000) return;
    s_power_checked = now; s_power_has_checked = true;
    char text[64];
    uint8_t reg = 0, status[2], percent = 255;
    if(!s_pmu || i2c_master_transmit_receive(s_pmu, &reg, 1, status, 2, 100) != ESP_OK) {
        snprintf(text, sizeof(text), LV_SYMBOL_BATTERY_EMPTY " --%%");
    } else if(!(status[0] & (1 << 3))) {
        bool usb = (status[0] & (1 << 5)) && !(status[1] & (1 << 3));
        snprintf(text, sizeof(text), "%s", usb ? "USB" : "NO BATTERY");
    } else {
        reg = 0xA4;
        bool valid = i2c_master_transmit_receive(s_pmu, &reg, 1, &percent, 1, 100) == ESP_OK && percent <= 100;
        const char *icon = !valid || percent < 10 ? LV_SYMBOL_BATTERY_EMPTY :
            percent < 35 ? LV_SYMBOL_BATTERY_1 : percent < 60 ? LV_SYMBOL_BATTERY_2 :
            percent < 90 ? LV_SYMBOL_BATTERY_3 : LV_SYMBOL_BATTERY_FULL;
        bool charging = (status[1] >> 5) == 1;
        bool full = (status[1] & 7) == 4;
        char value[8];
        if(valid) snprintf(value, sizeof(value), "%u%%", percent);
        else snprintf(value, sizeof(value), "--%%");
        snprintf(text, sizeof(text), "%s  %s%s%s", icon, value,
                 charging ? "  " LV_SYMBOL_CHARGE : "", full ? "  FULL" : "");
    }
    if(strcmp(text, s_power_text) != 0) {
        snprintf(s_power_text, sizeof(s_power_text), "%s", text);
        lv_label_set_text(s_power, text);
    }
}

static void init_time(void)
{
    setenv("TZ", "CST-8", 1);
    tzset();
    /* Compiler's local time is a preview fallback, not network time sync. */
    struct tm t = {.tm_isdst = -1};
    char month[4]; int year, day, hour, minute, second;
    sscanf(__DATE__, "%3s %d %d", month, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);
    const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char *m = strstr(months, month);
    t.tm_year = year - 1900; t.tm_mon = m ? (m - months) / 3 : 0;
    t.tm_mday = day; t.tm_hour = hour; t.tm_min = minute; t.tm_sec = second;
    time_t seed = mktime(&t);
    i2c_device_config_t cfg = {.dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = 0x51, .scl_speed_hz = 100000};
    uint8_t reg = 0, data[11];
    bool valid = false;
    if(i2c_master_bus_add_device(bsp_i2c_get_handle(), &cfg, &s_rtc) == ESP_OK &&
       i2c_master_transmit_receive(s_rtc, &reg, 1, data, sizeof(data), 100) == ESP_OK) {
        valid = !(data[0] & 0x22) && !(data[4] & 0x80);
        if(valid) {
            t.tm_sec = from_bcd(data[4] & 0x7f); t.tm_min = from_bcd(data[5] & 0x7f);
            t.tm_hour = from_bcd(data[6] & 0x3f); t.tm_mday = from_bcd(data[7] & 0x3f);
            t.tm_mon = from_bcd(data[9] & 0x1f) - 1; t.tm_year = from_bcd(data[10]) + 100;
            valid = t.tm_sec < 60 && t.tm_min < 60 && t.tm_hour < 24 && t.tm_mday >= 1 &&
                    t.tm_mday <= 31 && t.tm_mon >= 0 && t.tm_mon < 12;
            if(valid) seed = mktime(&t);
        }
        if(!valid) {
            localtime_r(&seed, &t);
            uint8_t write_time[] = {4, to_bcd(t.tm_sec), to_bcd(t.tm_min), to_bcd(t.tm_hour),
                to_bcd(t.tm_mday), to_bcd(t.tm_wday), to_bcd(t.tm_mon + 1), to_bcd(t.tm_year % 100)};
            uint8_t ctrl[] = {0, (uint8_t)(data[0] & ~0x22)};
            esp_err_t err = i2c_master_transmit(s_rtc, write_time, sizeof(write_time), 100);
            if(err == ESP_OK) err = i2c_master_transmit(s_rtc, ctrl, sizeof(ctrl), 100);
            ESP_LOGW(TAG, "RTC initialized with build time (preview), result: %s", esp_err_to_name(err));
        }
    }
    struct timeval tv = {.tv_sec = seed};
    settimeofday(&tv, NULL);
    ESP_LOGI(TAG, "time source: %s", valid ? "PCF85063" : "build time preview; needs calibration");
}

static void update_labels(void)
{
    time_t now = time(NULL); struct tm t; localtime_r(&now, &t);
    if(t.tm_min == s_last_minute) return;
    s_last_minute = t.tm_min;
    char text[40];
    snprintf(text, sizeof(text), "%02d:%02d", t.tm_hour, t.tm_min);
    lv_label_set_text(s_time, text);
    static const char *days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    snprintf(text, sizeof(text), "%02d / %02d   %s", t.tm_mon + 1, t.tm_mday, days[t.tm_wday]);
    lv_label_set_text(s_date, text);
    /* Minute-by-minute movement spreads the lit pixels over a small area. */
    int dx = (t.tm_min % 3 - 1) * 4, dy = ((t.tm_min / 3) % 3 - 1) * 4;
    lv_obj_align(s_time, LV_ALIGN_CENTER, dx, -46 + dy);
    lv_obj_align(s_date, LV_ALIGN_CENTER, dx, 44 + dy);
    lv_obj_align(s_power, LV_ALIGN_CENTER, dx, 116 + dy);
}

static void show_clock(void)
{
    s_showing = true; s_blanked = false; s_clock_entered = lv_tick_get();
    pet_set_paused(true); s_last_minute = -1;
    update_power(true); update_labels(); lv_screen_load(s_clock_screen);
    bsp_display_brightness_set(10);
}
static void wake_cb(lv_event_t *e)
{
    (void)e;
    if(!s_showing) return;
    s_showing = false; s_blanked = false;
    lv_screen_load(s_pet_screen);
    lv_refr_now(NULL);
    bsp_display_brightness_set(50); pet_set_paused(false);
    lv_display_trigger_activity(NULL);
}
/* Official bundled XPowersLib: INTEN2=0x41, INTSTS2=0x49,
 * PKEY_SHORT_IRQ bit 11 => bit 3 in the second register. Only acknowledge
 * our short-press bit; preserve other IRQs and all power/charging settings. */
static void key_cb(lv_timer_t *timer)
{
    (void)timer;
    if(!s_pmu) return;
    uint8_t reg = 0x49, status;
    if(i2c_master_transmit_receive(s_pmu, &reg, 1, &status, 1, 20) != ESP_OK || !(status & 8)) return;
    uint8_t clear[] = {0x49, 8};
    if(i2c_master_transmit(s_pmu, clear, sizeof(clear), 20) != ESP_OK) return;
    if(s_showing) wake_cb(NULL);
    else show_clock();
}
static void tick_cb(lv_timer_t *timer)
{
    (void)timer;
    if(s_blanked) return;
    if(s_showing) {
        if(lv_tick_get() - s_clock_entered >= 60000) {
            /* Black pixels + zero AMOLED brightness, not CPU deep sleep.
             * Keep PMU polling alive so the power key can wake the UI. */
            lv_screen_load(s_black_screen); lv_refr_now(NULL);
            bsp_display_backlight_off(); s_blanked = true;
        } else { update_labels(); update_power(false); }
    }
    else if(lv_display_get_inactive_time(NULL) >= 15000) show_clock();
}
void watch_clock_create(lv_obj_t *pet_screen)
{
    init_time(); s_pet_screen = pet_screen;
    s_clock_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_clock_screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_clock_screen, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(s_clock_screen, false);
    lv_obj_add_event_cb(s_clock_screen, wake_cb, LV_EVENT_PRESSED, NULL);
    s_time = lv_label_create(s_clock_screen);
    lv_obj_set_style_text_font(s_time, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_time, lv_color_hex(0xDADADA), 0);
    lv_obj_set_style_transform_scale(s_time, 512, 0);
    lv_obj_set_style_transform_pivot_x(s_time, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(s_time, lv_pct(50), 0);
    s_date = lv_label_create(s_clock_screen);
    lv_obj_set_style_text_font(s_date, &lv_font_montserrat_26, 0);
    lv_obj_set_style_transform_scale(s_date, 315, 0);
    lv_obj_set_style_transform_pivot_x(s_date, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(s_date, lv_pct(50), 0);
    lv_obj_set_style_text_color(s_date, lv_color_hex(0xBBBBBB), 0);
    s_power = lv_label_create(s_clock_screen);
    lv_obj_set_style_text_font(s_power, &lv_font_montserrat_24, 0);
    lv_obj_set_style_transform_scale(s_power, 384, 0);
    lv_obj_set_style_transform_pivot_x(s_power, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(s_power, lv_pct(50), 0);
    lv_obj_set_style_text_color(s_power, lv_color_hex(0xBDBDBD), 0);
    s_black_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_black_screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_black_screen, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(s_black_screen, false);
    i2c_device_config_t pmu_cfg = {.dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x34, .scl_speed_hz = 100000};
    esp_err_t pmu_err = i2c_master_bus_add_device(bsp_i2c_get_handle(), &pmu_cfg, &s_pmu);
    if(pmu_err != ESP_OK) ESP_LOGW(TAG, "battery monitoring unavailable: %s", esp_err_to_name(pmu_err));
    else {
        uint8_t reg = 0x41, enabled;
        if(i2c_master_transmit_receive(s_pmu, &reg, 1, &enabled, 1, 100) == ESP_OK) {
            uint8_t clear[] = {0x49, 8}, enable[] = {0x41, (uint8_t)(enabled | 8)};
            esp_err_t err = i2c_master_transmit(s_pmu, clear, sizeof(clear), 100);
            if(err == ESP_OK) err = i2c_master_transmit(s_pmu, enable, sizeof(enable), 100);
            if(err != ESP_OK) ESP_LOGW(TAG, "power key setup failed: %s", esp_err_to_name(err));
        }
    }
    lv_timer_create(key_cb, 100, NULL);
    lv_timer_create(tick_cb, 1000, NULL);
    show_clock(); /* Immediate preview after startup. */
}
