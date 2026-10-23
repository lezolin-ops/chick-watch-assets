/* Native two-window preview of the firmware's actual widgets and pet scene.
 * Fixed demonstration data; no hardware/network/storage access. */
#include "lvgl.h"
#include "pet.h"
#include "watch_home_clock.h"
#include "watch_power.h"
#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
esp_err_t watch_power_client_create(const char *n,unsigned r,watch_power_client_t *p){(void)n;(void)r;*p=(void*)1;return 0;}
esp_err_t watch_power_client_acquire(watch_power_client_t p){(void)p;return 0;}
esp_err_t watch_power_client_release(watch_power_client_t p){(void)p;return 0;}
bool watch_steps_get(uint32_t *s,bool *f){*s=10000;*f=false;return true;}
static uint32_t mask_pixels[410*502];
static lv_image_dsc_t mask={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_ARGB8888,.w=410,.h=502,.stride=410*4},.data_size=410*502*4,.data=(const uint8_t*)mask_pixels};
static void corners(lv_obj_t *screen)
{
    lv_obj_t *image=lv_image_create(screen);lv_image_set_src(image,&mask);
    lv_obj_set_clickable(image,false);lv_obj_set_pos(image,0,0);
}
int main(void)
{
    lv_init();
    for(int y=0;y<502;y++)for(int x=0;x<410;x++) {
        if((x<111||x>299)&&(y<111||y>391)) {
            int dx=x-(x<111?111:299),dy=y-(y<111?111:391);
            if(dx*dx+dy*dy>111*111)mask_pixels[y*410+x]=0xff232323;
        }
    }
    struct tm t={.tm_hour=8,.tm_min=30,.tm_mon=9,.tm_mday=18,.tm_wday=0};
    lv_display_t *home=lv_sdl_window_create(410,502);lv_display_set_default(home);
    lv_sdl_window_set_title(home,"Main - DEMO 10000 steps / 75%");
    SDL_SetWindowPosition(lv_sdl_window_get_window(home),650,120);
    pet_create(lv_screen_active(),410,502);
    watch_home_clock_create(lv_screen_active());watch_home_clock_update(&t,true);corners(lv_screen_active());
    lv_indev_t *mouse=lv_sdl_mouse_create();lv_indev_set_display(mouse,home);
    lv_display_t *standby=lv_sdl_window_create(410,502);lv_display_set_default(standby);
    lv_sdl_window_set_title(standby,"Standby - DEMO 10000 STEP / 75%");
    SDL_SetWindowPosition(lv_sdl_window_get_window(standby),220,120);
    lv_obj_t *screen=lv_screen_active();lv_obj_set_style_bg_color(screen,lv_color_black(),0);
    lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);lv_obj_set_scrollable(screen,false);
    watch_standby_face_create(screen);watch_standby_face_update(&t,true);corners(screen);
    watch_status_set_power(LV_SYMBOL_BATTERY_3 "  75%");
    SDL_RaiseWindow(lv_sdl_window_get_window(home));
    for(;;){uint32_t delay=lv_timer_handler();SDL_Delay(delay==LV_NO_TIMER_READY?20:LV_MIN(delay,20));}
}
