/* 电子宠物真机入口：BSP 初始化显示/触摸/LVGL → pet_create */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_err.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "pet.h"
#include "watch_clock.h"
#include "watch_settings.h"
#include "watch_power.h"
#include "watch_battery_log.h"
#include "watch_idle_diag.h"
#include "watch_steps.h"

static const char *TAG = "pet_watch";

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    bsp_display_start();
    ESP_LOGI(TAG, "display started, creating pet");

    /* 2026-10-11：USB 供电（无电池）场景下背光 100% 全亮电流过大 → 电压跌落导致
       SPI 传输失败 + USB 掉线（见 FIRMWARE.md §3.13）。降为 50% 减载；装电池后可调回 */
    bsp_display_brightness_set(50);

    bsp_display_lock(0);
    pet_create(lv_screen_active(), 410, 502);
    watch_clock_create(lv_screen_active());
    watch_settings_init();
    err = watch_power_init();
    if(err != ESP_OK) ESP_LOGW(TAG, "Power optimization unavailable: %s", esp_err_to_name(err));
    err = watch_battery_log_init();
    if(err != ESP_OK) ESP_LOGW(TAG, "Battery recording unavailable: %s", esp_err_to_name(err));
    watch_idle_diag_snapshot(0);
    err = watch_steps_init();
    if(err != ESP_OK) ESP_LOGW(TAG, "Step counting unavailable: %s", esp_err_to_name(err));
    bsp_display_unlock();
}
