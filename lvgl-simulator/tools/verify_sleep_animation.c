#define main existing_verifier_main
#include "verify_daily_animation.c"
#undef main
static void advance(unsigned ms) {for(unsigned i=0;i<ms;i+=20){lv_tick_inc(20);lv_timer_handler();}}
static void shot(lv_display_t*d,unsigned i){char path[512];snprintf(path,sizeof(path),"/tmp/sleep_%03u.ppm",i);save_preview(d,path);}
int main(void){
 lv_init();lv_display_t*d=lv_display_create(410,502);
 lv_display_set_color_format(d,LV_COLOR_FORMAT_ARGB8888);
 lv_display_set_buffers(d,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(d,flush);
 pet_create(lv_screen_active(),410,502);unsigned frame=0;
 for(unsigned i=0;i<10;i++){advance(100);shot(d,frame++);}
 pet_enter_sleep();assert(sleep_phase==SLEEP_ENTER);
 unsigned started=lv_tick_get();
 for(unsigned i=0;i<18;i++){pet_click_cb(NULL);assert(!s_pet.happy);advance(100);shot(d,frame++);}
 assert(sleep_phase==SLEEP_LOOP);assert(lv_tick_elaps(started)==1800);
 uint32_t daily_started=s_pet.started;int x=s_pet.x;unsigned min=256,max=0;
 for(unsigned i=0;i<66;i++){advance(100);shot(d,frame++);int v=lv_obj_get_style_transform_scale_y(s_pet.img,0);if(v<min)min=v;if(v>max)max=v;
 blink_timer_cb(NULL);mouth_timer_cb(NULL);routine_timer_cb(NULL);pet_click_cb(NULL);
 assert(!s_pet.happy&&!s_pet.head_turning&&!s_pet.front_flapping&&!s_pet.calling);assert(s_pet.x==x&&s_pet.started==daily_started);assert(!s_pet.awake);}
 assert(min==254&&max==256);pet_exit_sleep();assert(sleep_phase==SLEEP_EXIT);assert(lv_obj_get_style_transform_scale_y(s_pet.img,0)==256);
 for(unsigned i=0;i<14;i++){pet_click_cb(NULL);assert(!s_pet.happy);advance(100);shot(d,frame++);}
 assert(sleep_phase==SLEEP_NONE&&s_pet.phase==REST);assert(s_pet.blink_stage==0);advance(2800);assert(s_pet.phase==REST&&!s_pet.head_turning);
 advance(50000);assert(s_pet.awake&&sleep_phase==SLEEP_NONE);
 for(int direction=-1;direction<=1;direction+=2){s_pet.x=0;enter_phase(OUT);s_pet.direction=direction;s_pet.from_x=0;s_pet.x=direction*5;show_walk(5,direction);pet_enter_sleep();assert(sleep_walk_ms<=400);advance(sleep_prep_ms+1520);assert(sleep_phase==SLEEP_LOOP);assert(s_pet.x==direction*12);pet_exit_sleep();advance(1420);assert(sleep_phase==SLEEP_NONE);}
 pet_enter_sleep();pet_exit_sleep();assert(sleep_phase==SLEEP_ENTER);advance(1820);assert(sleep_phase==SLEEP_EXIT);advance(1420);assert(sleep_phase==SLEEP_NONE);
 pet_set_state(PET_STATE_HUNGRY);pet_set_state(PET_STATE_SLEEP);advance(1820);assert(sleep_phase==SLEEP_LOOP);pet_set_state(PET_STATE_IDLE);advance(1420);assert(sleep_phase==SLEEP_NONE);
 for(int phase=REST;phase<=SIT;phase++) {
  s_pet.x=0;enter_phase((daily_phase_t)phase);advance(40);pet_enter_sleep();
  advance(sleep_prep_ms+1520);assert(sleep_phase==SLEEP_LOOP);assert(!s_pet.awake);
  pet_exit_sleep();advance(1420);assert(sleep_phase==SLEEP_NONE&&s_pet.phase==REST);
 }
 verify_gait();
 puts("PASS: sleep lifecycle, 1800ms seated entry, 1400ms exit, 254..256 breathing, quiet loop, ignored clicks, queued exit, two walking directions and hunger compatibility.");
}
