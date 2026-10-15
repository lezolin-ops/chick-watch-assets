#define main previous_main
#define PET_TEST_SOURCE "../../firmware/main/pet.c"
#include "../../../lvgl-simulator/tools/verify_daily_animation.c"
#undef main
static void advance(unsigned ms){for(unsigned i=0;i<ms;i+=20){lv_tick_inc(20);lv_timer_handler();}}
static void check_pause(void){
 lv_refr_now(NULL);uint32_t start=sleep_started;int scale=lv_obj_get_style_transform_scale_y(s_pet.img,0),nod=sleep_nod;pet_sleep_phase_t phase=sleep_phase;
 pet_set_paused(true);advance(10000);assert(sleep_phase==phase&&sleep_nod==nod);assert(lv_obj_get_style_transform_scale_y(s_pet.img,0)==scale);
 pet_set_paused(false);assert(sleep_started==start+10000);assert(lv_timer_get_paused(s_pet.timer));assert(lv_timer_get_paused(s_pet.blink_timer));assert(lv_timer_get_paused(s_pet.mouth_timer));
}
int main(void){lv_init();lv_display_t*d=lv_display_create(410,502);lv_display_set_color_format(d,LV_COLOR_FORMAT_ARGB8888);lv_display_set_buffers(d,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(d,flush);pet_create(lv_screen_active(),410,502);
 pet_enter_sleep();advance(1200);check_pause();advance(600);assert(sleep_phase==SLEEP_LOOP);advance(1200);check_pause();assert(lv_timer_get_paused(sleep_timer));
 pet_exit_sleep();advance(200);check_pause();advance(1200);assert(sleep_phase==SLEEP_NONE);assert(!lv_timer_get_paused(s_pet.timer));assert(!lv_timer_get_paused(s_pet.blink_timer));puts("PASS: ENTER/LOOP/EXIT freeze for ten seconds while hidden, animation phase preserved, normal timers stay paused until wake completes.");}
