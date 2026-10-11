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

    bsp_display_lock(0);
    pet_create(lv_screen_active(), 410, 502);
    bsp_display_unlock();
}
