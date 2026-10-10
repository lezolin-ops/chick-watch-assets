#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lvgl/src/misc/lv_timer_private.h"
#include "../src/pet.c"
static const char *output_dir;
static lv_point_t mouse_point;
static bool mouse_down;
static void pointer_read(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev; data->point=mouse_point; data->state=mouse_down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
}
static uint32_t buffer[410*502];
static uint32_t frame[410*502];
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p) {
    for(int y=a->y1; y<=a->y2; y++) memcpy(frame+y*410+a->x1, p+(y-a->y1)*(a->x2-a->x1+1)*4, (a->x2-a->x1+1)*4);
    lv_display_flush_ready(d);
}
static void step(unsigned ms) {for(unsigned i=0;i<ms;i++){lv_tick_inc(1);lv_timer_handler();}}
static void save(unsigned n) {
    if(output_dir == NULL) return;
    lv_refr_now(NULL);
    char name[512];snprintf(name,sizeof(name),"%s/%04u.bgra",output_dir,n);
    FILE *f=fopen(name,"wb");assert(f);fwrite(frame,sizeof(frame),1,f);fclose(f);
}
int main(int argc, char **argv) {
    output_dir = argc > 1 ? argv[1] : NULL;
    lv_init();
    lv_display_t *d=lv_display_create(410,502);
    lv_display_set_color_format(d,LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(d,flush);
    pet_create(lv_screen_active(),410,502);step(40);
    int ground=lv_obj_get_y(s_pet.pet);
    /* Record genuine LVGL rendering: normal pace, one blink, tap jump, next blink. */
    lv_timer_set_period(s_pet.wing_timer,1500);lv_timer_reset(s_pet.wing_timer);
    lv_timer_set_period(s_pet.blink_timer,1800);lv_timer_reset(s_pet.blink_timer);
    for(unsigned n=0;n<220;n++) {if(n==100)pet_set_state(PET_STATE_HAPPY);step(33);save(n);}
    assert(s_pet.state==PET_STATE_IDLE && lv_obj_get_y(s_pet.pet)==ground);
    /* Transitions at every blink phase must instantly remove eyelids. */
    for(int phase=0;phase<205;phase+=20){
        pet_set_state(PET_STATE_IDLE);blink_timer_cb(s_pet.blink_timer);step(phase);
        pet_set_state(PET_STATE_HAPPY);assert(s_pet.eyelid==0);step(650);
        assert(s_pet.state==PET_STATE_IDLE && s_pet.eyelid==0);
    }
    const pet_state_t other_states[] = {PET_STATE_WALK, PET_STATE_HUNGRY, PET_STATE_SLEEP};
    for(unsigned n=0;n<sizeof(other_states)/sizeof(other_states[0]);n++) {
        pet_set_state(PET_STATE_IDLE);blink_timer_cb(s_pet.blink_timer);step(90);
        pet_set_state(other_states[n]);assert(s_pet.eyelid==0);step(6000);assert(s_pet.eyelid==0);
    }
    pet_set_state(PET_STATE_IDLE);
    for(int n=0;n<20;n++){blink_timer_cb(s_pet.blink_timer);step(300);assert(s_pet.eyelid==0);assert(lv_obj_get_y(s_pet.pet)==ground);assert(s_pet.blink_timer->period>=2800 && s_pet.blink_timer->period<=5200);}
    /* Two small wing beats, with a pause between them; no body displacement. */
    pet_set_state(PET_STATE_IDLE);
    wing_timer_cb(s_pet.wing_timer);
    int peaks = 0, previous = 0; bool rising = true;
    for(int ms=0;ms<1200;ms+=10) {
        step(10); int angle=s_pet.wing_angle;
        assert(angle>=0 && angle<=180); assert(lv_obj_get_y(s_pet.pet)==ground);
        if(rising && angle<previous) {peaks++; rising=false;}
        if(!rising && angle>previous) rising=true;
        previous=angle;
    }
    assert(peaks==2 && s_pet.wing_angle==0);
    assert(s_pet.wing_timer->period>=6500 && s_pet.wing_timer->period<=11000);
    /* Interrupt each wing phase with a happy jump; reset both wings. */
    for(int phase=0;phase<900;phase+=90) {
        pet_set_state(PET_STATE_IDLE);wing_timer_cb(s_pet.wing_timer);step(phase);
        pet_set_state(PET_STATE_HAPPY);assert(s_pet.wing_angle==0);
        assert(lv_obj_is_hidden(s_pet.wings[0]) && lv_obj_is_hidden(s_pet.wings[1]));
        step(650);assert(s_pet.state==PET_STATE_IDLE && s_pet.wing_angle==0);
        assert(!lv_obj_is_hidden(s_pet.wings[0]));
    }
    for(unsigned n=0;n<sizeof(other_states)/sizeof(other_states[0]);n++) {
        pet_set_state(PET_STATE_IDLE);wing_timer_cb(s_pet.wing_timer);step(100);
        pet_set_state(other_states[n]);assert(s_pet.wing_angle==0);
        assert(lv_obj_is_hidden(s_pet.wings[0]));step(12000);assert(s_pet.wing_angle==0);
    }
    /* Alpha-aware clicks on each rotated wing still trigger the existing jump. */
    lv_indev_t *mouse=lv_indev_create();lv_indev_set_type(mouse,LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(mouse,d);lv_indev_set_read_cb(mouse,pointer_read);
    for(unsigned side=0;side<2;side++) {
        pet_set_state(PET_STATE_IDLE);wing_anim_cb(s_pet.pet,180);lv_obj_update_layout(s_pet.pet);
        lv_area_t area;lv_obj_get_coords(s_pet.wings[side],&area);
        mouse_point=(lv_point_t){area.x1+(side==0?9:18),area.y1+15};
        lv_obj_transform_point(s_pet.wings[side],&mouse_point,LV_OBJ_POINT_TRANSFORM_FLAG_RECURSIVE);
        mouse_down=true;lv_indev_read(mouse);step(50);mouse_down=false;lv_indev_read(mouse);step(50);
        assert(s_pet.state==PET_STATE_HAPPY);assert(s_pet.wing_angle==0);step(650);
        assert(s_pet.state==PET_STATE_IDLE);
    }
    lv_indev_delete(mouse);
    lv_obj_clean(lv_screen_active());step(6000);assert(s_pet.pet==NULL);
    pet_create(lv_screen_active(),410,502);step(6000);lv_obj_delete(s_pet.pet);step(6000);assert(s_pet.pet==NULL);
    puts("PASS: blink and two wing beats, jump interruptions, other states, fixed ground, timer/object cleanup");
}
