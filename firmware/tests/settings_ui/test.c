#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../main/watch_settings.c"
#include "../../main/watch_home_clock.c"
static watch_wifi_status_t fake;
static uint32_t fake_steps;
static bool steps_available=true, steps_save_failed=false;
bool watch_steps_get(uint32_t *steps,bool *saved){*steps=fake_steps;*saved=steps_save_failed;return steps_available;}
static bool test_offline_save_fail;
static bool test_wifi_toggle_fail;
int test_gpio_level=1;
static uint32_t pixels[410*502];

esp_err_t watch_wifi_init(void){return 0;}
esp_err_t watch_wifi_set_offline_auto_off(bool enabled){if(test_offline_save_fail)return -1;fake.offline_auto_off=enabled;return 0;}
esp_err_t watch_wifi_scan(void){return 0;}
esp_err_t watch_wifi_connect(const wifi_ap_record_t *ap,const char *p){(void)ap;(void)p;return 0;}
esp_err_t watch_wifi_set_enabled(bool enabled){if(test_wifi_toggle_fail)return -1;fake.enabled=enabled;return 0;}
void watch_wifi_status(watch_wifi_status_t *out){*out=fake;}
void watch_clock_settings_open(lv_obj_t *screen){lv_screen_load(screen);}
void watch_clock_settings_close(void){}
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){
 for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)pixels[y*410+x]=((uint32_t*)p)[(y-a->y1)*(a->x2-a->x1+1)+x-a->x1];
 lv_display_flush_ready(d);
}
static void image(const char *name){
 lv_obj_update_layout(s_screen);lv_refr_now(NULL);
 char path[512];snprintf(path,sizeof(path),"%s/%s.ppm",getenv("WATCH_TEST_OUTPUT"),name);
 FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n410 502\n255\n");
 for(unsigned i=0;i<410*502;i++){
  int x=i%410,y=i/410;uint8_t rgb[]={(pixels[i]>>16)&255,(pixels[i]>>8)&255,pixels[i]&255};
  if((x<111||x>299)&&(y<111||y>391)){
   int dx=x-(x<111?111:299),dy=y-(y<111?111:391);
   if(dx*dx+dy*dy>111*111)rgb[0]=rgb[1]=rgb[2]=35;
  }
  fwrite(rgb,1,3,f);
 }fclose(f);
}
static void safe_point(int x,int y){
 assert(x>=8&&x<=401&&y>=8&&y<=493);
 if((x<111||x>299)&&(y<111||y>391)){
  int dx=x-(x<111?111:299),dy=y-(y<111?111:391);
  assert(dx*dx+dy*dy<=103*103);
 }
}
static void safe_control(lv_obj_t *obj){
 lv_area_t a;lv_obj_get_coords(obj,&a);
 safe_point(a.x1,a.y1);safe_point(a.x2,a.y1);safe_point(a.x1,a.y2);safe_point(a.x2,a.y2);
}
static void safe_page(void){
 lv_obj_update_layout(s_screen);
 safe_control(lv_obj_get_child(s_screen,0));
 for(unsigned i=0;i<lv_obj_get_child_count(s_content);i++){
  lv_obj_t *child=lv_obj_get_child(s_content,i);
  if(lv_obj_check_type(child,&lv_button_class)||lv_obj_check_type(child,&lv_textarea_class))safe_control(child);
 }
 if(s_keyboard)safe_control(s_keyboard);
}
static void key(unsigned index){lv_buttonmatrix_set_selected_button(s_keyboard,index);lv_obj_send_event(s_keyboard,LV_EVENT_VALUE_CHANGED,NULL);}
int main(void){
 lv_init();lv_display_t *d=lv_display_create(410,502);lv_display_set_color_format(d,LV_COLOR_FORMAT_XRGB8888);
 fake.enabled=true;
 lv_draw_buf_t *draw=lv_draw_buf_create(410,502,LV_COLOR_FORMAT_XRGB8888,LV_STRIDE_AUTO);assert(draw);
 lv_display_set_draw_buffers(d,draw,NULL);lv_display_set_render_mode(d,LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(d,flush);
 lv_obj_t *pet=lv_screen_active();watch_home_clock_create(pet);
 struct tm t={.tm_hour=23,.tm_min=59,.tm_mon=11,.tm_mday=31,.tm_wday=4};
 watch_home_clock_update(&t,true);lv_obj_update_layout(pet);
 assert(!strcmp(lv_label_get_text(s_time),"23:59"));
 safe_control(s_steps);
 assert(!strcmp(lv_label_get_text(s_steps),"0 步"));
 fake_steps=UINT32_MAX;steps_save_failed=true;watch_home_clock_update(&t,true);lv_obj_update_layout(pet);safe_control(s_steps);
 lv_point_t step_size;lv_text_get_size(&step_size,lv_label_get_text(s_steps),&watch_step_numbers_30,0,0,1000,0);assert(step_size.x<=290);
 steps_available=false;watch_home_clock_update(&t,true);assert(!strcmp(lv_label_get_text(s_steps),"计步暂不可用"));
 steps_available=true;fake_steps=1234;steps_save_failed=false;watch_home_clock_update(&t,true);

 assert(!strcmp(lv_label_get_text(s_date),"12月31日 周四"));safe_control(s_time);safe_control(s_date);
 t.tm_hour=0;t.tm_min=0;t.tm_mon=0;t.tm_mday=1;t.tm_wday=5;
 watch_home_clock_update(&t,true);assert(!strcmp(lv_label_get_text(s_date),"1月1日 周五"));
 for(int month=0;month<12;month++)for(int day=1;day<=31;day++)for(int weekday=0;weekday<7;weekday++){
  t.tm_mon=month;t.tm_mday=day;t.tm_wday=weekday;watch_home_clock_update(&t,true);
  lv_point_t size;lv_text_get_size(&size,lv_label_get_text(s_date),&watch_date_26,0,0,1000,0);assert(size.x<=290);
  lv_obj_update_layout(pet);safe_control(s_date);
 }
 watch_home_clock_update(&t,false);assert(!strcmp(lv_label_get_text(s_time),"--:--"));assert(!strlen(lv_label_get_text(s_date)));
 for(int h=0;h<24;h++)for(int m=0;m<60;m++){
  t.tm_hour=h;t.tm_min=m;watch_home_clock_update(&t,true);lv_obj_update_layout(pet);
  lv_point_t size;lv_text_get_size(&size,lv_label_get_text(s_time),&watch_cartoon_68,0,0,1000,0);assert(size.x<=290);safe_control(s_time);
 }
 s_screen=pet;lv_obj_set_style_bg_color(pet,lv_color_hex(0x73CFF3),0);
 t.tm_hour=8;t.tm_min=30;t.tm_mon=9;t.tm_mday=12;t.tm_wday=1;watch_home_clock_update(&t,true);image("home-clock");
 fake_steps=UINT32_MAX;steps_save_failed=true;watch_home_clock_update(&t,true);lv_obj_update_layout(pet);safe_control(s_steps);image("steps-save-failed");
 steps_available=false;watch_home_clock_update(&t,true);lv_obj_update_layout(pet);safe_control(s_steps);image("steps-unavailable");
 steps_available=true;steps_save_failed=false;fake_steps=1234;watch_home_clock_update(&t,true);
 /* Both faces: real widgets, numeric extremes, battery/error states and
  * every minute movement. Check actual full widget bounds under the mask. */
 const char *batteries[]={LV_SYMBOL_BATTERY_3 "  75%",LV_SYMBOL_BATTERY_EMPTY " --%","USB","NO BATTERY",LV_SYMBOL_BATTERY_FULL "  100%  " LV_SYMBOL_CHARGE "  FULL"};
 lv_obj_t *standby=lv_obj_create(NULL);lv_obj_set_style_bg_color(standby,lv_color_black(),0);
 watch_standby_face_create(standby);lv_screen_load(standby);s_screen=standby;
 for(unsigned battery=0;battery<sizeof(batteries)/sizeof(batteries[0]);battery++) {
  watch_status_set_power(batteries[battery]);
  for(unsigned state=0;state<4;state++) {
   fake_steps=state==0?0:state==1?10000:UINT32_MAX;
   steps_available=state!=3;steps_save_failed=state==2;
   for(int minute=0;minute<9;minute++) {
    t.tm_min=minute;watch_home_clock_update(&t,true);watch_standby_face_update(&t,true);
    lv_obj_update_layout(standby);safe_control(s_face_row);safe_control(s_pill);safe_control(s_pill_text);
    lv_area_t a;lv_obj_get_coords(s_face_time,&a);
    int cx=(a.x1+a.x2)/2,cy=(a.y1+a.y2)/2;
    safe_point(cx-lv_obj_get_width(s_face_time),cy-lv_obj_get_height(s_face_time));
    safe_point(cx+lv_obj_get_width(s_face_time),cy+lv_obj_get_height(s_face_time));
    lv_obj_update_layout(pet);safe_control(s_steps);safe_control(s_home_power);safe_control(s_date);
   }
  }
 }
 steps_available=true;steps_save_failed=false;fake_steps=10000;
 t.tm_hour=8;t.tm_min=30;t.tm_mon=9;t.tm_mday=18;t.tm_wday=0;
 watch_status_set_power(batteries[0]);watch_standby_face_update(&t,true);image("standby-status");
 steps_save_failed=true;watch_standby_face_update(&t,true);image("standby-save-failed");
 steps_available=false;watch_standby_face_update(&t,true);image("standby-unavailable");
 watch_standby_face_update(NULL,false);assert(!strcmp(lv_label_get_text(s_face_time),"--:--"));
 steps_available=true;steps_save_failed=false;watch_home_clock_update(&t,true);
 s_screen=pet;lv_screen_load(pet);image("home-status");
 watch_settings_init();s_active=true;lv_screen_load(s_screen);page(HOME);safe_page();image("settings");
 lv_obj_t *row=lv_obj_get_child(s_content,2);
 lv_obj_t *sw=lv_obj_get_child(row,3);safe_control(sw);
 lv_obj_send_event(sw,LV_EVENT_VALUE_CHANGED,NULL);assert(!s_home_clock_enabled&&test_nvs_clock==0&&lv_obj_is_hidden(s_time));
 assert(lv_obj_is_hidden(s_steps)&&!lv_obj_is_hidden(s_home_power));
 assert(!lv_obj_is_hidden(s_pill));
 s_home_clock_enabled=true;watch_settings_init();assert(!s_home_clock_enabled);lv_screen_load(s_screen);page(HOME);
 test_nvs_fail=-1;lv_obj_send_event(lv_obj_get_child(s_content,2),LV_EVENT_CLICKED,NULL);
 assert(s_home_clock_save_failed&&!s_home_clock_enabled&&test_nvs_clock==0);safe_page();image("clock-save-failed");
 test_nvs_fail=0;lv_obj_send_event(lv_obj_get_child(s_content,2),LV_EVENT_CLICKED,NULL);
 assert(s_home_clock_enabled&&test_nvs_clock==1&&!lv_obj_is_hidden(s_time));
 assert(!lv_obj_is_hidden(s_steps)&&!lv_obj_is_hidden(s_home_power));safe_page();image("settings-clock-on");
 fake.count=3;strcpy((char*)fake.aps[0].ssid,"Home Wi-Fi");fake.aps[0].authmode=WIFI_AUTH_WPA2_PSK;
 strcpy((char*)fake.aps[1].ssid,"Studio");strcpy((char*)fake.aps[2].ssid,"Guest");page(NETWORK);safe_page();
 for(unsigned i=0;i<2;i++){lv_obj_t *toggle=lv_obj_get_child(lv_obj_get_child(s_content,i),3);safe_control(toggle);assert(lv_obj_check_type(toggle,&lv_switch_class));}
 image("network");
 test_wifi_toggle_fail=true;lv_obj_send_event(lv_obj_get_child(s_content,0),LV_EVENT_CLICKED,NULL);assert(fake.enabled&&s_wifi_toggle_failed);assert(lv_obj_has_state(lv_obj_get_child(lv_obj_get_child(s_content,0),3),LV_STATE_CHECKED));safe_page();image("network-switch-failed");test_wifi_toggle_fail=false;
 lv_obj_send_event(lv_obj_get_child(s_content,0),LV_EVENT_CLICKED,NULL);assert(!fake.enabled);safe_page();
 lv_obj_send_event(lv_obj_get_child(s_content,0),LV_EVENT_CLICKED,NULL);assert(fake.enabled);safe_page();
 lv_obj_send_event(lv_obj_get_child(s_content,1),LV_EVENT_CLICKED,NULL);assert(fake.offline_auto_off);assert(lv_obj_has_state(lv_obj_get_child(lv_obj_get_child(s_content,1),3),LV_STATE_CHECKED));safe_page();image("network-offline-on");
 test_offline_save_fail=true;lv_obj_send_event(lv_obj_get_child(s_content,1),LV_EVENT_CLICKED,NULL);assert(fake.offline_auto_off&&s_offline_save_failed);safe_page();image("network-offline-save-failed");test_offline_save_fail=false;
 lv_obj_t *policy_switch=lv_obj_get_child(lv_obj_get_child(s_content,1),3);lv_obj_remove_state(policy_switch,LV_STATE_CHECKED);lv_obj_send_event(policy_switch,LV_EVENT_VALUE_CHANGED,NULL);assert(!fake.offline_auto_off);assert(!lv_obj_has_state(lv_obj_get_child(lv_obj_get_child(s_content,1),3),LV_STATE_CHECKED));safe_page();
 fake.enabled=false;fake.auto_stopped=true;page(NETWORK);safe_page();image("network-auto-stopped");fake.enabled=true;fake.auto_stopped=false;
 s_selected=fake.aps[0];page(PASSWORD);safe_page();image("password");
 key(0);assert(!strcmp(lv_textarea_get_text(s_password),"q"));
 key(19);assert(lv_keyboard_get_mode(s_keyboard)==LV_KEYBOARD_MODE_TEXT_UPPER);key(0);
 assert(!strcmp(lv_textarea_get_text(s_password),"qQ"));key(28);key(0);
 assert(!strcmp(lv_textarea_get_text(s_password),"qQ1"));
 lv_keyboard_set_mode(s_keyboard,LV_KEYBOARD_MODE_USER_1);key(0);key(29);
 assert(!strcmp(lv_textarea_get_text(s_password),"qQ1"));
 lv_textarea_set_text(s_password,"validpass");connect_selected();assert(s_page==RESULT&&s_waiting);
 fake.state=WATCH_WIFI_CONNECTED;strcpy(fake.ssid,"Home Wi-Fi");strcpy(fake.message,"下次开机会自动连接");s_waiting=false;
 page(RESULT);safe_page();image("connected");
 page(BRIGHTNESS);image("brightness");
 for(unsigned i=0;i<lv_obj_get_child_count(s_content);i++){
  lv_obj_t *child=lv_obj_get_child(s_content,i);
  if(lv_obj_check_type(child,&lv_slider_class)){
   assert(lv_slider_get_max_value(child)==100);lv_slider_set_value(child,100,LV_ANIM_OFF);
   lv_obj_send_event(child,LV_EVENT_VALUE_CHANGED,NULL);lv_obj_send_event(child,LV_EVENT_RELEASED,NULL);
   assert(watch_settings_brightness()==100);
  }
 }
 watch_settings_back();assert(s_page==HOME);watch_settings_back();assert(!s_active);
 lv_tick_inc(100);test_gpio_level=0;tick(NULL);lv_tick_inc(100);test_gpio_level=1;tick(NULL);
 assert(s_active&&s_page==HOME);
 page(PASSWORD);watch_settings_exit();assert(!s_active&&!s_password&&!s_keyboard);
 puts("PASS: all 1440 times fit, midnight rollover, invalid time, persisted switch and save failure; keyboard lower/upper/numeric/symbol/delete, connect, BOOT debounce/return, clear password on exit, both status faces, battery/step extremes, transformed clock and rounded boundaries, rendered screens");
}
