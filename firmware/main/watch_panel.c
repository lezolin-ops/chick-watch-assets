#include "watch_panel.h"
#include "bsp/display.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
static esp_lcd_panel_io_handle_t s_io;
static lv_timer_t *s_refresh;
static bool s_asleep,s_restore;
static const char *TAG="watch_panel";
static esp_err_t command(unsigned cmd,const void *data,size_t size) {
    /* Same 32-bit QSPI command framing as BSP and SH8601 tx_param. */
    return esp_lcd_panel_io_tx_param(s_io,(0x02U<<24)|(cmd<<8),data,size);
}
static void delay_ms(unsigned ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
esp_err_t watch_panel_init(void) {
    esp_lcd_panel_handle_t panel;
    esp_err_t err=bsp_display_get_panel_io(&panel,&s_io);
    if(err!=ESP_OK){s_io=NULL;return err;}
    s_refresh=lv_display_get_refr_timer(lv_display_get_default());
    if(!s_refresh){s_io=NULL;return ESP_ERR_INVALID_STATE;}
    return ESP_OK;
}
bool watch_panel_is_asleep(void) { return s_asleep; }
esp_err_t watch_panel_wake(void) {
    if(!s_io||!s_restore)return ESP_OK;
    esp_err_t err=command(0x11,NULL,0);
    if(err!=ESP_OK){delay_ms(120);return err;}
    delay_ms(120); /* CO5300 V0.01 pp145-146: booster on transition. */
    uint8_t zero=0;
    err=command(0x51,&zero,1); /* Remain dark until caller redraws. */
    if(err==ESP_OK)err=command(0x29,NULL,0);
    if(err!=ESP_OK)return err;
    delay_ms(20);
    s_asleep=false;s_restore=false;
    lv_timer_resume(s_refresh);
    ESP_LOGI(TAG,"SLPOUT/DISPON complete; refresh resumed at zero brightness");
    return ESP_OK;
}
esp_err_t watch_panel_sleep(void) {
    if(!s_io)return ESP_ERR_INVALID_STATE;
    if(s_asleep)return ESP_OK;
    uint8_t zero=0;
    esp_err_t err=command(0x51,&zero,1);
    if(err!=ESP_OK)return err;
    lv_timer_pause(s_refresh);s_restore=true;
    /* tx_param drains queued DMA transfers before the polling command. */
    err=command(0x28,NULL,0);
    if(err==ESP_OK){delay_ms(20);err=command(0x10,NULL,0);}
    if(err!=ESP_OK){
        delay_ms(120); /* A bus error may occur after SLPIN was accepted. */
        esp_err_t restore=watch_panel_wake();
        ESP_LOGW(TAG,"Sleep command failed: %s; recovery=%s",esp_err_to_name(err),esp_err_to_name(restore));
        return err;
    }
    delay_ms(120); /* pp143-144; prevent early SLPOUT after SLPIN. */
    s_asleep=true;
    ESP_LOGI(TAG,"DISPOFF/SLPIN complete; refresh paused");
    return ESP_OK;
}
