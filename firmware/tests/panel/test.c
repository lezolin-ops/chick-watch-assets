#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "lvgl.h"
#include "esp_lcd_panel_io.h"
static unsigned commands[128],count,delayed,fail_command=999;
static bool available=true;
const char *esp_err_to_name(int err){return err?"fail":"ok";}
void vTaskDelay(unsigned ticks){delayed+=ticks;}
esp_err_t bsp_display_get_panel_io(esp_lcd_panel_handle_t *p,esp_lcd_panel_io_handle_t *io){if(!available)return ESP_ERR_INVALID_STATE;*p=(void*)1;*io=(void*)2;return ESP_OK;}
esp_err_t esp_lcd_panel_io_tx_param(esp_lcd_panel_io_handle_t io,int command,const void *data,size_t n){
 assert(io==(void*)2);assert(((unsigned)command&0xff0000ffU)==0x02000000U);
 unsigned cmd=((unsigned)command>>8)&255;commands[count++]=cmd;
 if(cmd==0x51){assert(n==1&&*(const uint8_t*)data==0);}else assert(!n&&!data);
 if(cmd==fail_command){fail_command=999;return ESP_FAIL;}return ESP_OK;
}
#include "../../main/watch_panel.c"
int main(void){
 lv_init();lv_display_t *display=lv_display_create(410,502);assert(display);
 available=false;assert(watch_panel_init()==ESP_ERR_INVALID_STATE);assert(watch_panel_wake()==ESP_OK);
 available=true;assert(watch_panel_init()==ESP_OK);
 assert(watch_panel_sleep()==ESP_OK&&watch_panel_is_asleep());assert(lv_timer_get_paused(s_refresh));assert(count==3&&commands[0]==0x51&&commands[1]==0x28&&commands[2]==0x10);assert(delayed==140);
 assert(watch_panel_sleep()==ESP_OK&&count==3);
 assert(watch_panel_wake()==ESP_OK&&!watch_panel_is_asleep());assert(!lv_timer_get_paused(s_refresh));assert(count==6&&commands[3]==0x11&&commands[4]==0x51&&commands[5]==0x29);assert(delayed==280);
 assert(watch_panel_wake()==ESP_OK&&count==6);
 for(unsigned i=0;i<3;i++){fail_command=(unsigned[]){0x51,0x28,0x10}[i];assert(watch_panel_sleep()==ESP_FAIL);assert(!watch_panel_is_asleep());assert(!s_restore);assert(!lv_timer_get_paused(s_refresh));}
 assert(watch_panel_sleep()==ESP_OK);fail_command=0x11;assert(watch_panel_wake()==ESP_FAIL&&watch_panel_is_asleep());assert(watch_panel_wake()==ESP_OK&&!watch_panel_is_asleep());
 assert(watch_panel_sleep()==ESP_OK);fail_command=0x29;assert(watch_panel_wake()==ESP_FAIL&&s_restore);assert(watch_panel_wake()==ESP_OK&&!s_restore);
 puts("PASS: real LVGL refresh timer, QSPI framing, sleep/wake order, delays, repeated calls, failed enter/exit and recovery");
 return 0;
}
