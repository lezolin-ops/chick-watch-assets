#define main existing_verifier_main
#include "verify_daily_animation.c"
#undef main
int main(void) {
 lv_init();lv_display_t*d=lv_display_create(410,502);
 lv_display_set_color_format(d,LV_COLOR_FORMAT_ARGB8888);
 lv_display_set_buffers(d,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_FULL);
 lv_display_set_flush_cb(d,flush);pet_create(lv_screen_active(),410,502);
 stop_breathing_animation(s_pet.pet,s_pet.img);
 lv_timer_pause(s_pet.timer);lv_timer_pause(s_pet.blink_timer);lv_timer_pause(s_pet.mouth_timer);
 lv_obj_update_layout(s_pet.img);lv_area_t origin;lv_obj_get_coords(s_pet.img,&origin);
 s_pet.blink_stage=0;render_eyes();save_preview(d,"/tmp/blink-open.ppm");
 memcpy(mouth_baseline,screen_buffer,sizeof(screen_buffer));
 for(int stage=1;stage<=2;stage++) {
  s_pet.blink_stage=stage;render_eyes();
  save_preview(d,stage==1 ? "/tmp/blink-half.ppm" : "/tmp/blink-closed.ppm");
  int inside=0,outside=0;
  for(int y=0;y<502;y++)for(int x=0;x<410;x++) {
   int offset=(y*410+x)*4;
   if(!memcmp(mouth_baseline+offset,screen_buffer+offset,4))continue;
   bool eye=false;for(int i=0;i<2;i++) {
    const eye_region_t*e=&s_pet.eyes->eyes[i];
    if(x>=origin.x1+e->x-1 && x<=origin.x1+e->x+e->w && y>=origin.y1+e->y-1 && y<=origin.y1+e->y+e->h)eye=true;
   }
   if(eye)inside++;else outside++;
  }
  printf("stage %d: %d changed eye pixels, %d outside\n",stage,inside,outside);assert(inside>0 && outside==0);
 }
 breath_scale_y_cb(s_pet.img,252);
 for(int stage=0;stage<3;stage++) {s_pet.blink_stage=stage;render_eyes();assert(lv_obj_get_style_transform_scale_y(s_pet.img,0)==252);}
 s_pet.blinking=false;s_pet.blink_started=lv_tick_get();s_pet.blink_wait=2000;
 lv_tick_inc(2000);blink_timer_cb(NULL);assert(s_pet.blink_stage==1);
 lv_tick_inc(40);blink_timer_cb(NULL);assert(s_pet.blink_stage==2);
 lv_tick_inc(60);blink_timer_cb(NULL);assert(s_pet.blink_stage==1);
 lv_tick_inc(80);blink_timer_cb(NULL);assert(s_pet.blink_stage==0 && s_pet.blink_wait>=2000 && s_pet.blink_wait<=5000);
 puts("PASS: local blink only; breathing scale untouched; original 40/60/80ms stages and random 2..5s preserved");
}
