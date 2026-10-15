#define main existing_verifier_main
#include "verify_daily_animation.c"
#undef main
static void advance(unsigned ms) {for(unsigned i=0;i<ms;i+=20){lv_tick_inc(20);lv_timer_handler();}}
int main(void) {
 lv_init();lv_display_t*d=lv_display_create(410,502);
 lv_display_set_color_format(d,LV_COLOR_FORMAT_ARGB8888);
 lv_display_set_buffers(d,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(d,flush);
 pet_create(lv_screen_active(),410,502);
 for(int phase=REST;phase<=SIT;phase++) {
  advance(2600);enter_phase((daily_phase_t)phase);
  uint32_t start=s_pet.started, duration=s_pet.duration;
  int x=s_pet.x;assert(pet_play_happy_once());uint32_t trigger=s_pet.happy_started;
  assert(lv_anim_get(s_pet.jump,happy_progress_cb));
  for(unsigned i=0;i<80;i++) {
   assert(!pet_play_happy_once());advance(20);
   assert(s_pet.phase==phase && s_pet.x==x && s_pet.duration==duration);
   unsigned t=lv_tick_elaps(trigger);
   if(t>=80 && t<=100) {
    assert(lv_obj_get_style_transform_scale_y(s_pet.pet,0)<256);
    assert(lv_obj_get_style_translate_y(s_pet.jump,0)>=1);
   }
   if(t>=200 && t<=260) assert(lv_obj_get_style_transform_scale_y(s_pet.pet,0)>256);
   if(t>=200 && t<=1300) {
    assert(s_pet.happy_smiling);
    for(unsigned eye=0;eye<s_pet.eyes->count;eye++) {
     assert(!lv_obj_is_hidden(s_pet.eye_lines[eye]));
     assert(s_pet.eyelid_points[eye][2].y<s_pet.eyelid_points[eye][0].y);
    }
   }
   if(phase==REST && i==4) save_preview(d,"/tmp/mini_happy_crouch.ppm");
   if(phase==REST && i==37) save_preview(d,"/tmp/mini_happy_landing.ppm");
   if(phase==REST && i==20) save_preview(d,"/tmp/mini_happy_face.ppm");
   lv_obj_update_layout(s_pet.pet);
   lv_area_t a;lv_obj_get_coords(s_pet.pet,&a);
   /* Include foot-pivot scaling and round outward for the full touch rectangle. */
   int sx=lv_obj_get_style_transform_scale_x(s_pet.pet,0);
   int sy=lv_obj_get_style_transform_scale_y(s_pet.pet,0);
   int ox=a.x1,oy=a.y1;
   a.x1=ox+96+(int)floorf(-96*sx/256.0f);
   a.x2=ox+96+(int)ceilf(95*sx/256.0f);
   a.y1=oy+170+(int)floorf(-170*sy/256.0f);
   a.y2=oy+170+(int)ceilf(21*sy/256.0f);
   for(int edge=0;edge<4;edge++) {
    int px=(edge&1)?a.x2:a.x1, py=(edge&2)?a.y2:a.y1;
    assert(px>=8 && px<=402 && py>=8 && py<=494);
    if((px<111 || px>299) && (py<111 || py>391)) {
     int dx=px-(px<111?111:299),dy=py-(py<111?111:391);assert(dx*dx+dy*dy<=103*103);
    }
   }
  }
  advance(40);assert(!s_pet.happy);assert(!s_pet.happy_smiling);assert(!lv_anim_get(s_pet.jump,happy_progress_cb));assert(s_pet.started-start>=1600);
  assert(lv_obj_get_style_translate_y(s_pet.jump,0)==0);
 assert(lv_obj_get_style_transform_scale_x(s_pet.pet,0)==256);
 assert(lv_obj_get_style_transform_scale_y(s_pet.pet,0)==256);
  assert(!pet_play_happy_once());advance(900);assert(lv_tick_elaps(trigger)>=2500);
 }
 pet_set_state(PET_STATE_HUNGRY);assert(pet_play_happy_once());advance(1640);
 assert(lv_timer_get_paused(s_pet.timer));
 advance(2600);assert(pet_play_happy_once());pet_enter_sleep();assert(!s_pet.happy);
 assert(lv_obj_get_style_translate_y(s_pet.jump,0)==0);
 assert(lv_obj_get_style_transform_scale_x(s_pet.pet,0)==256);
 assert(lv_obj_get_style_transform_scale_y(s_pet.pet,0)==256);assert(!pet_play_happy_once());
 puts("PASS: 12 phases preserved, 1600ms one-shot, repeated clicks/cooldown, restored transforms, hungry timer, sleep cancellation, touch bounds.");
}
