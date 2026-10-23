/** Grounded daily behavior, driven by pose frames and horizontal walking. */
#include "pet.h"
#include "pet_chick_hungry.c"
#include "pet_chick_sleep.c"
#include "bg_morning_meadow_home.c"
#include "pet_daily_sprites.c"
#include "pet_walk_rig_assets.c"
#include "pet_blink_geometry.h"
#include "pet_rest_head_assets.c"
#include "pet_rest_head_eyes.h"
#include "pet_sky_head_assets.c"
#include "pet_sky_head_eyes.h"
#include "pet_ground_head_assets.c"
#include "pet_ground_head_eyes.h"
#include "pet_walk_gait.h"
#include "pet_mouth_geometry.h"
#include "pet_beak_assets.c"
#include "pet_side_mouth_bodies.c"
#include "pet_side_body_eyes.h"
#include <string.h>
#include <stdio.h>
#include "pet_happy_frames.c" /* simulator preview only: AI happy keyframes */
#include "pet_happy_seq.c" /* simulator preview only: AI smooth sequence frames (not firmware) */
#include "pet_angry_seq.c" /* simulator preview only: angry keyframe loop (not firmware) */
#include "pet_material_palette.h"
#include "pet_wing_frames.c"
#include "pet_wing_animation.h"
#include "pet_front_flap_patches.c"
#include "pet_front_wing.h"
#include "pet_walk_expression.h"
#include "pet_scene.h"
#include "pet_tuft.h"

/* Keep illustrated bitmap feet; animate each foot independently by distance. */

typedef enum {
    REST, RISE, STAND, LOOK, OUT, SETTLE_OUT, TURN, BACK, SETTLE_BACK, FACE, PAUSE, SIT
} daily_phase_t;

typedef struct {
    lv_obj_t *pet;
    lv_obj_t *jump;
    lv_obj_t *img;
    lv_obj_t *shadow;
    lv_obj_t *feet[2]; /* near/left and far/right keep their identities */
    lv_obj_t *wing;
    unsigned wing_frame,wing_settle_start;
    const lv_image_dsc_t *front_base;
    bool front_seated,front_flapping,front_flap_used,front_flap_enabled;
    uint32_t front_flap_started,front_flap_wait;
    walk_foot_pose_t foot_poses[2];
    lv_timer_t *timer;
    lv_timer_t *blink_timer;
    lv_timer_t *mouth_timer;
    const mouth_pose_t *mouth;
    uint32_t mouth_started, mouth_wait;
    uint8_t mouth_stage;
    bool calling;
    bool side_portrait;
    bool head_turning;
    bool sky_chosen,sky_used,head_is_sky;
    bool ground_chosen,ground_used,head_is_ground;
    bool will_walk,head_has_returned;
    uint32_t head_returned;
    uint32_t head_started,head_wait,head_hold;
    int head_direction;
    unsigned head_frame;
    int gaze_offset;
    int gaze_y;
    lv_obj_t *eye_glances[2];
    lv_image_dsc_t eye_glance_images[2];
    uint8_t eye_glance_pixels[2][32*32*4];
    lv_obj_t *eye_covers[2];
    lv_obj_t *eye_pupils[2];
    lv_obj_t *eye_lines[2];
    lv_point_precise_t eyelid_points[2][5];
    const eye_pose_t *eyes;
    uint32_t blink_started, blink_wait;
    uint8_t blink_stage;
    bool blinking, awake;
    int eye_direction;
    daily_phase_t phase;
    uint32_t started, duration;
    int32_t x, from_x, target_x, excursion;
    int direction;
    bool walk_pause;
    tuft_motion_t tuft;
    uint32_t walk_pause_ms,walk_first_ms;
    float walk_checkpoint;
    bool happy;
    uint32_t happy_started, happy_last;
    bool happy_used, happy_timer_paused, happy_smiling;
    uint8_t happy_blink_stage;
} pet_runtime_t;

static pet_runtime_t s_pet;
static pet_sleep_phase_t sleep_phase = SLEEP_NONE;
static lv_timer_t *sleep_timer;
static uint32_t sleep_started, sleep_prep_ms, sleep_walk_ms;
static bool sleep_exit_pending, sleep_was_walking, sleep_was_seated;
static float sleep_walk_from, sleep_walk_to;
static int sleep_walk_direction;
static int32_t sleep_nod;
static bool sleep_drowsy_started, sleep_was_calling;
static uint32_t sleep_call_elapsed;
static unsigned sleep_head_start;
static uint8_t sleep_head_pixels[192*192*4];
static lv_image_dsc_t sleep_head_image;
static void sleep_tick(lv_timer_t *timer);


/* Breathing owns img Y compression and pet Y offset only.
 * Happy hops use a separate parent so they cannot overwrite breathing. */
static void breath_scale_y_cb(void *var, int32_t value)
{
    lv_obj_set_style_transform_scale_y(var, value, 0);
}
static void breath_y_cb(void *var, int32_t value)
{
    lv_obj_set_style_translate_y(var, value, 0);
}
static void stop_breathing_animation(lv_obj_t *pet, lv_obj_t *img)
{
    lv_anim_delete(img, breath_scale_y_cb);
    lv_anim_delete(pet, breath_y_cb);
    breath_scale_y_cb(img,256);
    breath_y_cb(pet,0);
}
static void start_breathing_animation(lv_obj_t *pet, lv_obj_t *img)
{
    /* Preserve phase across quiet-state changes and happy clicks. */
    if(lv_anim_get(img,breath_scale_y_cb)) return;
    stop_breathing_animation(pet,img);
    lv_obj_set_style_transform_pivot_x(img,96,0);
    lv_obj_set_style_transform_pivot_y(img,170,0);
    lv_anim_t scale;
    lv_anim_init(&scale);
    lv_anim_set_var(&scale,img);
    lv_anim_set_exec_cb(&scale,breath_scale_y_cb);
    lv_anim_set_values(&scale,256,252);
    lv_anim_set_duration(&scale,1600);
    lv_anim_set_reverse_duration(&scale,1600);
    lv_anim_set_path_cb(&scale,lv_anim_path_ease_in_out);
    lv_anim_set_repeat_count(&scale,LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&scale);
    lv_anim_t offset;
    lv_anim_init(&offset);
    lv_anim_set_var(&offset,pet);
    lv_anim_set_exec_cb(&offset,breath_y_cb);
    lv_anim_set_values(&offset,0,0);
    lv_anim_set_duration(&offset,1600);
    lv_anim_set_reverse_duration(&offset,1600);
    lv_anim_set_path_cb(&offset,lv_anim_path_ease_in_out);
    lv_anim_set_repeat_count(&offset,LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&offset);
}
static void set_breathing(bool enabled)
{
    if(enabled)start_breathing_animation(s_pet.pet,s_pet.img);
    else stop_breathing_animation(s_pet.pet,s_pet.img);
}

static const lv_image_dsc_t * const frames[2][12] = {
    { &pet_daily_right_0, &pet_daily_right_1, &pet_daily_right_2,
      &pet_daily_right_3, &pet_daily_right_4, &pet_daily_right_5,
      &pet_daily_right_6, &pet_daily_right_7, &pet_daily_right_8,
      &pet_daily_right_9, &pet_daily_right_10, &pet_daily_right_11 },
    { &pet_daily_left_0, &pet_daily_left_1, &pet_daily_left_2,
      &pet_daily_left_3, &pet_daily_left_4, &pet_daily_left_5,
      &pet_daily_left_6, &pet_daily_left_7, &pet_daily_left_8,
      &pet_daily_left_9, &pet_daily_left_10, &pet_daily_left_11 }
};
static const lv_image_dsc_t * const foot_images[2][2] = {
    { &pet_rig_foot_near_right, &pet_rig_foot_far_right },
    { &pet_rig_foot_near_left, &pet_rig_foot_far_left }
};
static const lv_image_dsc_t * const side_bodies[2][3] = {
    { &pet_side_body_0_right, &pet_side_body_1_right, &pet_side_body_2_right },
    { &pet_side_body_0_left, &pet_side_body_1_left, &pet_side_body_2_left }
};
static void render_eyes(void);
static void refresh_side_portrait(void);
static uint32_t palette_skin_color(int canonical_x,int y)
{
    const lv_image_dsc_t *image=lv_img_get_src(s_pet.img);
    for(int dy=0;dy<=4;dy++) for(int dx=0;dx<=6;dx++) {
        int x=canonical_x-dx, yy=y+dy;
        if(s_pet.eye_direction<0) x=191-x;
        if(x<0 || x>=192 || yy<0 || yy>=192) continue;
        const uint8_t *p=image->data+yy*image->header.stride+x*4;
        if(p[3]>200 && p[2]>230 && p[1]>185 && p[0]<135)
            return (p[2]<<16)|(p[1]<<8)|p[0];
    }
    return 0xFCD53B;
}

static uint8_t mouth_mask_data[192*192];
static const lv_image_dsc_t mouth_mask={
    .header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_A8,
             .flags=LV_IMAGE_FLAGS_MODIFIABLE,.w=192,.h=192,.stride=192},
    .data_size=sizeof(mouth_mask_data),.data=mouth_mask_data,
};
static const lv_image_dsc_t * const beak_images[2][3][3]={
    {
        { &pet_beak_0_0_right, &pet_beak_0_1_right, &pet_beak_0_2_right },
        { &pet_beak_1_0_right, &pet_beak_1_1_right, &pet_beak_1_2_right },
        { &pet_beak_2_0_right, &pet_beak_2_1_right, &pet_beak_2_2_right },
    },
    {
        { &pet_beak_0_0_left, &pet_beak_0_1_left, &pet_beak_0_2_left },
        { &pet_beak_1_0_left, &pet_beak_1_1_left, &pet_beak_1_2_left },
        { &pet_beak_2_0_left, &pet_beak_2_1_left, &pet_beak_2_2_left },
    },
};

static void update_mouth_mask(void)
{
    const mouth_pose_t *m=s_pet.mouth;
    if(m==NULL || (!s_pet.awake && sleep_phase!=SLEEP_LOOP) || m->count==0 || m->mode==2) {
        lv_obj_set_style_bitmap_mask_src(s_pet.img,NULL,0);
        return;
    }
    memset(mouth_mask_data,255,sizeof(mouth_mask_data));
    for(unsigned i=0;i<m->count;i++) {
        const mouth_row_t *r=&m->rows[i];
        int x1=s_pet.eye_direction<0 ? 191-r->x2 : r->x1;
        int x2=s_pet.eye_direction<0 ? 191-r->x1 : r->x2;
        memset(mouth_mask_data+r->y*192+x1,0,x2-x1+1);
    }
    lv_obj_set_style_bitmap_mask_src(s_pet.img,&mouth_mask,0);
}

static void mouth_draw_cb(lv_event_t *event)
{
    const mouth_pose_t *m=s_pet.mouth;
    if((!s_pet.awake && sleep_phase!=SLEEP_LOOP) || m==NULL || m->count==0 || m->mode==2) return;
    lv_layer_t *layer=lv_event_get_layer(event);
    lv_area_t origin;
    lv_obj_get_coords(s_pet.img,&origin);
    int direction=s_pet.eye_direction<0 ? -1 : 1;
    int scale_y=lv_obj_get_style_transform_scale_y(s_pet.img,0);
    lv_draw_rect_dsc_t fill;
    lv_draw_rect_dsc_init(&fill);
    fill.bg_opa=LV_OPA_COVER;
    for(unsigned i=0;i<m->count;i++) {
        const mouth_row_t *r=&m->rows[i];
        int row_y=origin.y1+170+(int)lroundf((r->y-170)*scale_y/256.0f);
        int last=LV_MIN(r->x2,r->skin_x2);
        if(last>=r->x1) {
            int x1=direction<0 ? 191-last : r->x1;
            int x2=direction<0 ? 191-r->x1 : last;
            fill.bg_color=lv_color_hex(palette_skin_color(m->x-2,r->y));
            lv_area_t a={origin.x1+x1,row_y,origin.x1+x2,row_y};
            lv_draw_rect(layer,&fill,&a);
        }
        if(m->mode==2 && r->skin_x2>=r->x1 && r->skin_x2<=r->x2) {
            int x=direction<0 ? 191-r->skin_x2 : r->skin_x2;
            fill.bg_color=lv_color_hex(0xE99D1B);
            lv_area_t a={origin.x1+x,row_y,origin.x1+x,row_y};
            lv_draw_rect(layer,&fill,&a);
        }
    }
    /* Embed the profile bill root into the restored cheek, not its outer edge. */
    int attachment_x=m->cx-(m->mode==2 ? 3 : 0);
    int cx=origin.x1+(direction<0 ? 191-attachment_x : attachment_x);
    int anchor=m->mode==2 ? 4 : 16;
    if(direction<0) anchor=31-anchor;
    lv_draw_image_dsc_t art;
    lv_draw_image_dsc_init(&art);
    art.src=pet_color_image(beak_images[direction<0][m->mode][s_pet.mouth_stage]);
    art.scale_y=scale_y;
    art.pivot.x=0;
    art.pivot.y=170-(m->cy-10);
    lv_area_t a={cx-anchor,origin.y1+m->cy-10,cx-anchor+31,origin.y1+m->cy+25};
    lv_draw_image(layer,&art,&a);
}

static bool mouth_can_call(void)
{
    uint32_t phase_elapsed=lv_tick_elaps(s_pet.started);
    return !s_pet.head_turning && !s_pet.front_flapping &&
           (!s_pet.head_has_returned || lv_tick_elaps(s_pet.head_returned)>=1200) &&
           phase_elapsed<s_pet.duration && s_pet.duration-phase_elapsed>1200 &&
           s_pet.awake && s_pet.mouth != NULL &&
           (s_pet.phase == REST || s_pet.phase == STAND || s_pet.phase == PAUSE);
}

static void mouth_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if(s_pet.happy || sleep_phase!=SLEEP_NONE || !s_pet.awake || s_pet.mouth == NULL) return;
    uint32_t elapsed = lv_tick_elaps(s_pet.mouth_started);
    if(!s_pet.calling && elapsed >= s_pet.mouth_wait && mouth_can_call()) {
        s_pet.calling = true;
        s_pet.mouth_started = lv_tick_get();
        elapsed = 0;
    }
    uint8_t stage = 0;
    if(s_pet.calling) {
        stage = elapsed < 100 ? 1 : elapsed < 240 ? 2 : elapsed < 340 ? 1 : 0;
        if(elapsed >= 400) {
            s_pet.calling = false;
            s_pet.mouth_started = lv_tick_get();
            s_pet.mouth_wait = lv_rand(10000, 20000);   /* 轻叫 10~20s（原 20~40s） */
        }
    }
    if(stage != s_pet.mouth_stage) {
        s_pet.mouth_stage = stage;
        refresh_side_portrait();
        lv_obj_invalidate(s_pet.img);
    }
}

/* Local cover/eyelid overlays only: never change body scale or translation here. */
static void render_eyes(void)
{
    for(unsigned i = 0; i < 2; i++) {
        bool visible = s_pet.awake && (s_pet.happy_smiling || s_pet.blink_stage != 0 || s_pet.gaze_offset != 0 || s_pet.gaze_y != 0) && s_pet.eyes != NULL && i < s_pet.eyes->count;
        lv_obj_set_hidden(s_pet.eye_covers[i], !visible);
        if(!visible) continue;
        const eye_region_t *e = &s_pet.eyes->eyes[i];
        int x = s_pet.eye_direction < 0 ? 192 - e->x - e->w : e->x;
        bool glance=!s_pet.happy_smiling && (s_pet.gaze_offset!=0 || s_pet.gaze_y!=0) && s_pet.blink_stage==0;
        lv_obj_set_pos(s_pet.eye_covers[i], x-(glance ? 2 : 0), e->y+(int)lroundf(2.0f*sleep_nod/256)-(glance ? 2 : 0));
        lv_obj_set_size(s_pet.eye_covers[i], e->w+(glance ? 4 : 0), e->h+(glance ? 4 : 0));
        lv_obj_set_style_bg_color(s_pet.eye_covers[i],lv_color_hex(palette_skin_color(e->x-2,e->y+e->h/2)),0);
        bool closed = s_pet.happy_smiling || s_pet.blink_stage == 2;
        lv_obj_set_hidden(s_pet.eye_glances[i],!glance);
        lv_obj_set_hidden(s_pet.eye_pupils[i], closed || glance);
        lv_obj_set_hidden(s_pet.eye_lines[i], !closed);
        if(glance) {
            const lv_image_dsc_t *source=lv_img_get_src(s_pet.img);
            lv_image_dsc_t *crop=&s_pet.eye_glance_images[i];
            *crop=*source;
            crop->header.w=e->w; crop->header.h=e->h;
            crop->header.stride=e->w*4;
            crop->header.flags|=LV_IMAGE_FLAGS_MODIFIABLE;
            crop->data=s_pet.eye_glance_pixels[i];
            crop->data_size=e->h*crop->header.stride;
            for(int row=0;row<e->h;row++)
                memcpy(s_pet.eye_glance_pixels[i]+row*crop->header.stride,
                       source->data+(e->y+row)*source->header.stride+x*4,crop->header.stride);
            lv_img_set_src(s_pet.eye_glances[i],crop);
            lv_obj_set_pos(s_pet.eye_glances[i],2+s_pet.gaze_offset,2+s_pet.gaze_y);
        }
        else if(!closed) {
            lv_obj_set_size(s_pet.eye_pupils[i], e->w - 4, LV_MAX(3, e->h / 3));
            lv_obj_set_pos(s_pet.eye_pupils[i], 2, e->h / 2);
        }
        else {
            for(unsigned j = 0; j < 5; j++) {
                s_pet.eyelid_points[i][j].x = 2 + (e->w - 4) * j / 4;
                /* Raised middle makes an unmistakable happy eye arch, distinct from sleep. */
                int curve = j == 2 ? -4 : (j == 0 || j == 4 ? 2 : -2);
                s_pet.eyelid_points[i][j].y = e->h / 2 + (s_pet.happy_smiling ? curve : (j == 2 ? 2 : (j == 0 || j == 4 ? 0 : 1)));
            }
            lv_line_set_points(s_pet.eye_lines[i], s_pet.eyelid_points[i], 5);
        }
    }
}

static void refresh_side_portrait(void)
{
    if(!s_pet.side_portrait) return;
    lv_obj_set_hidden(s_pet.wing,true);
    lv_img_set_src(s_pet.img,pet_color_image(side_bodies[s_pet.eye_direction<0][s_pet.mouth_stage]));
    s_pet.eyes=&side_body_eyes[s_pet.mouth_stage];
    render_eyes();
}

static void blink_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if(s_pet.happy || sleep_phase!=SLEEP_NONE || !s_pet.awake) return;
    uint32_t elapsed = lv_tick_elaps(s_pet.blink_started);
    if(!s_pet.blinking && elapsed >= s_pet.blink_wait) {
        s_pet.blinking = true;
        s_pet.blink_started = lv_tick_get();
        elapsed = 0;
    }
    uint8_t stage = 0;
    if(s_pet.blinking) {
        stage = elapsed < 40 ? 1 : elapsed < 100 ? 2 : elapsed < 180 ? 1 : 0;
        if(elapsed >= 180) {
            s_pet.blinking = false;
            s_pet.blink_started = lv_tick_get();
            s_pet.blink_wait = lv_rand(2000, 5000);
        }
    }
    if(stage != s_pet.blink_stage) {
        s_pet.blink_stage = stage;
        render_eyes();
    }
}

static void show_frame(unsigned frame, int direction)
{
    lv_obj_set_hidden(s_pet.wing,true);
    s_pet.front_flapping=false;
    lv_obj_set_pos(s_pet.img,0,0);
    lv_image_set_rotation(s_pet.img,0);
    int shadow_width=frame==0 || frame==11 ? 76 : 54;
    lv_obj_set_size(s_pet.shadow,shadow_width,8);
    lv_obj_set_pos(s_pet.shadow,(192-shadow_width)/2,174);
    s_pet.side_portrait=false;
    for(unsigned foot = 0; foot < 2; foot++) lv_obj_set_hidden(s_pet.feet[foot], true);
    lv_img_set_src(s_pet.img,pet_color_image( frames[direction < 0][frame]));
    s_pet.front_base=(frame==0 || frame==3) ? pet_color_image(frames[direction<0][frame]) : NULL;
    s_pet.front_seated=frame==0;
    s_pet.eyes = &daily_eyes[frame];
    s_pet.mouth = &daily_mouths[frame];
    s_pet.eye_direction = direction;
    render_eyes();
    update_mouth_mask();
}

static void show_wing_frame(unsigned frame,int direction)
{
    s_pet.wing_frame=frame;
    lv_obj_set_hidden(s_pet.wing,false);
    lv_img_set_src(s_pet.wing,wing_frame_material(wing_animation[direction<0][frame]));
    lv_obj_set_pos(s_pet.wing,direction<0 ? 109 : 19,74);
    lv_image_set_pivot(s_pet.wing,direction<0 ? 9 : 54,34);
    lv_image_set_rotation(s_pet.wing,0); /* The motion is drawn into each frame. */
}

static void show_walk(float distance, int direction)
{
    s_pet.front_flapping=false;
    s_pet.front_base=NULL;
    lv_obj_set_size(s_pet.shadow,38,8);
    float support_x=walk_support_x(distance);
    lv_obj_set_pos(s_pet.shadow,96+direction*(int)lroundf(support_x*0.5f)-19,174);
    s_pet.side_portrait=true;
    const lv_image_dsc_t *body=pet_color_image(side_bodies[direction<0][s_pet.mouth_stage]);
    lv_img_set_src(s_pet.img,walk_expressive_body(body,distance,direction));
    lv_obj_set_pos(s_pet.img,0,(int)lroundf(walk_body_y(distance)));
    lv_image_set_pivot(s_pet.img,96,154);
    lv_image_set_rotation(s_pet.img,(int)lroundf(direction*WALK_BODY_TILT*sinf(6.2831853f*distance/WALK_STRIDE_PX)));
    lv_obj_invalidate(s_pet.img);
    show_wing_frame(wing_walk_frame(distance),direction);
    for(unsigned foot = 0; foot < 2; foot++) {
        walk_foot_pose_t pose = walk_foot_pose(distance, foot);
        s_pet.foot_poses[foot] = pose;
        lv_obj_t *image = s_pet.feet[foot];
        lv_obj_set_hidden(image, false);
        lv_img_set_src(image, pet_color_image(foot_images[direction < 0][foot]));
        int pivot_x = direction < 0 ? 30 : 18;
        int ankle_x = 96 + (int)lroundf(pose.x);
        if(direction < 0) ankle_x = 192 - ankle_x;
        lv_obj_set_pos(image, ankle_x - pivot_x, 138 - (int)lroundf(pose.lift));
        lv_image_set_pivot(image, pivot_x, 26);
        lv_image_set_rotation(image, (int)lroundf(direction < 0 ? -pose.angle : pose.angle));
    }
    s_pet.eyes = &side_body_eyes[s_pet.mouth_stage];
    s_pet.mouth = &rig_body_mouth;
    s_pet.eye_direction = direction;
    render_eyes();
    update_mouth_mask();
}

static void show_sky_head(unsigned frame)
{
    if(frame==0) { show_frame(0,1); return; }
    static const lv_image_dsc_t * const portraits[3]={&pet_sky_head_0,&pet_sky_head_1,&pet_sky_head_2};
    lv_img_set_src(s_pet.img,portraits[frame-1]);
    s_pet.eyes=&sky_head_eyes[frame-1];
    s_pet.eye_direction=1;
    s_pet.mouth=NULL;
    render_eyes();
    update_mouth_mask();
}

static void show_ground_head(unsigned frame)
{
    if(frame==0) { show_frame(0,1); return; }
    static const lv_image_dsc_t * const portraits[3]={&pet_ground_head_0,&pet_ground_head_1,&pet_ground_head_2};
    lv_img_set_src(s_pet.img,portraits[frame-1]);
    s_pet.eyes=&ground_head_eyes[frame-1];
    s_pet.eye_direction=1;
    s_pet.mouth=NULL;
    render_eyes();
    update_mouth_mask();
}

static void show_rest_head(unsigned frame,int direction)
{
    if(s_pet.head_is_sky) { show_sky_head(frame); return; }
    if(s_pet.head_is_ground) { show_ground_head(frame); return; }
    if(frame==0) { show_frame(0,1); return; }
    static const lv_image_dsc_t * const portraits[2][3]={
        {&pet_rest_head_0_right,&pet_rest_head_1_right,&pet_rest_head_2_right},
        {&pet_rest_head_0_left,&pet_rest_head_1_left,&pet_rest_head_2_left}
    };
    lv_img_set_src(s_pet.img,portraits[direction<0][frame-1]);
    s_pet.eyes=&rest_head_eyes[frame-1];
    s_pet.eye_direction=direction;
    s_pet.mouth=NULL; /* Closed illustrated bill; no competing mouth overlay. */
    render_eyes();
    update_mouth_mask();
}

static void rest_head_tick(uint32_t elapsed)
{
    if(!s_pet.head_turning) {
        if(elapsed<s_pet.head_wait || s_pet.calling || s_pet.front_flapping ||
           s_pet.duration-elapsed<3000 ||
           (s_pet.head_has_returned && lv_tick_elaps(s_pet.head_returned)<1200)) return;
        s_pet.head_turning=true;
        s_pet.head_started=elapsed;
        s_pet.head_is_sky=s_pet.sky_chosen && !s_pet.sky_used;
        s_pet.head_is_ground=!s_pet.head_is_sky && s_pet.ground_chosen && !s_pet.ground_used;
        if(s_pet.head_is_sky) s_pet.sky_used=true;
        if(s_pet.head_is_ground) s_pet.ground_used=true;
        s_pet.head_hold=s_pet.head_is_sky ? lv_rand(1000,2000) : s_pet.head_is_ground ? lv_rand(700,1100) : lv_rand(900,2400);
        s_pet.head_direction=lv_rand(0,1) ? 1 : -1;
        s_pet.head_frame=0;
    }
    uint32_t t=elapsed-s_pet.head_started;
    unsigned frame;
    if(t<200) {
        float u=LV_MIN(1.0f,t/100.0f);
        int look=(int)lroundf(2*u*u*(3-2*u));
        s_pet.gaze_offset=(s_pet.head_is_sky || s_pet.head_is_ground) ? 0 : s_pet.head_direction*look;
        s_pet.gaze_y=s_pet.head_is_sky ? -look : s_pet.head_is_ground ? look : 0;
        render_eyes();
        return;
    }
    s_pet.gaze_offset=0;
    s_pet.gaze_y=0;
    if(t<320) frame=1;
    else if(t<440) frame=2;
    else if(t<600+s_pet.head_hold) frame=3;
    else if(t<700+s_pet.head_hold) frame=2;
    else if(t<880+s_pet.head_hold) frame=1;
    else {
        s_pet.head_turning=false;
        s_pet.head_has_returned=true;
        s_pet.head_returned=lv_tick_get();
        s_pet.head_wait=elapsed+lv_rand(3000,6000);   /* 转头回正后 3~6s 再转（原 5~9s） */
        frame=0;
    }
    if(frame!=s_pet.head_frame) {
        s_pet.head_frame=frame;
        show_rest_head(frame,s_pet.head_direction);
    }
}

static void front_wing_tick(uint32_t elapsed)
{
    if(!s_pet.front_base) return;
    if(!s_pet.front_flapping) {
        if(!s_pet.front_flap_enabled || s_pet.front_flap_used || elapsed<s_pet.front_flap_wait ||
           s_pet.head_turning || s_pet.calling || s_pet.duration-elapsed<2500 ||
           (s_pet.head_has_returned && lv_tick_elaps(s_pet.head_returned)<1200)) return;
        s_pet.front_flapping=true; s_pet.front_flap_used=true; s_pet.front_flap_started=elapsed;
    }
    uint32_t t=elapsed-s_pet.front_flap_started;
    if(t>=WALK_CYCLE_MS) {
        s_pet.front_flapping=false;
        s_pet.head_has_returned=true; s_pet.head_returned=lv_tick_get();
        lv_img_set_src(s_pet.img,s_pet.front_base);
        render_eyes(); update_mouth_mask();
        return;
    }
    unsigned frame=wing_walk_frame(t*(WALK_STRIDE_PX/(float)WALK_CYCLE_MS));
    lv_img_set_src(s_pet.img,front_body_pose(s_pet.front_base,s_pet.front_seated,frame));
    render_eyes(); update_mouth_mask();
}

static void enter_phase(daily_phase_t phase)
{
    set_breathing(phase == REST || phase == STAND || phase == PAUSE);
    s_pet.phase = phase;
    s_pet.started = lv_tick_get();
    s_pet.head_turning=false;
    s_pet.gaze_offset=0;
    s_pet.gaze_y=0;
    s_pet.front_flapping=false; s_pet.front_flap_used=false;
    s_pet.front_flap_enabled=(phase==REST || phase==STAND) && lv_rand(0,99)<35;
    s_pet.front_flap_wait=phase==REST ? lv_rand(10000,18000) : lv_rand(1200,2000);
    if(phase!=REST && phase!=STAND && phase!=PAUSE) {
        s_pet.calling=false;
        s_pet.mouth_stage=0;
    }
    switch(phase) {
        case REST: {
            s_pet.duration = lv_rand(12000, 20000);   /* 窝坐 12~20s（原 24~45s） */
            s_pet.head_wait=lv_rand(2500,4500);       /* 首次转头等待 2.5~4.5s（原 4~7s） */
            s_pet.head_has_returned=false;
            s_pet.head_is_sky=false;
            s_pet.head_is_ground=false;
            unsigned observation=lv_rand(0,99);
            s_pet.sky_chosen=observation<10;
            s_pet.ground_chosen=observation>=10 && observation<30;
            s_pet.sky_used=false;
            s_pet.ground_used=false;
            s_pet.will_walk=lv_rand(0,99)<70;
            show_frame(0, 1);
            break;
        }
        case RISE:
            s_pet.duration = 600;
            show_frame(1, 1);
            break;
        case STAND:
            s_pet.duration = lv_rand(3000, 6000);
            show_frame(3, 1);
            break;
        case LOOK:
            s_pet.direction = lv_rand(0, 1) ? 1 : -1;
            /* Whole half-cycles end on a grounded two-foot contact. */
            s_pet.excursion = s_pet.direction * (int32_t)lv_rand(4, 5) * (WALK_STRIDE_PX / 2);
            s_pet.duration = 540;
            show_frame(4, s_pet.direction);
            break;
        case OUT:
        case BACK:
            s_pet.calling = false;
            s_pet.mouth_stage = 0;
            s_pet.from_x = s_pet.x;
            s_pet.target_x = phase == OUT ? s_pet.excursion : 0;
            s_pet.direction = s_pet.target_x > s_pet.from_x ? 1 : -1;
            s_pet.duration = (uint32_t)LV_ABS(s_pet.target_x - s_pet.from_x) * WALK_CYCLE_MS / WALK_STRIDE_PX;
            if(s_pet.duration == 0) s_pet.duration = 1;
            s_pet.walk_pause=phase==OUT && LV_ABS(s_pet.target_x-s_pet.from_x)>=48 && lv_rand(0,99)<25;
            s_pet.walk_pause_ms=s_pet.walk_pause ? lv_rand(600,1000) : 0;
            s_pet.walk_checkpoint=WALK_STRIDE_PX;
            s_pet.walk_first_ms=WALK_CYCLE_MS;
            s_pet.duration+=s_pet.walk_pause_ms;
            show_walk(0, s_pet.direction);
            break;
        case TURN:
            s_pet.duration = 840;
            show_frame(5, s_pet.direction);
            break;
        case SETTLE_OUT:
        case SETTLE_BACK:
            /* Preserve the final grounded pose before changing orientation. */
            s_pet.duration = 180;
            s_pet.wing_settle_start=s_pet.wing_frame;
            break;
        case FACE:
            s_pet.duration = 540;
            show_frame(5, s_pet.direction);
            break;
        case PAUSE:
            s_pet.duration = lv_rand(1000, 2000);
            show_frame(3, 1);
            break;
        case SIT:
            s_pet.duration = 600;
            show_frame(2, 1);
            break;
    }
}

static void update_tuft(void)
{
    if(!s_pet.awake || !s_pet.eyes || !s_pet.eyes->count) return;
    float x=0,y=0;
    for(unsigned i=0;i<s_pet.eyes->count;i++) {
        const eye_region_t *eye=&s_pet.eyes->eyes[i];
        float cx=eye->x+eye->w*0.5f;
        x+=s_pet.eye_direction<0 ? 191-cx : cx;
        y+=eye->y+eye->h*0.5f;
    }
    x/=s_pet.eyes->count;
    y=y/s_pet.eyes->count+lv_obj_get_y(s_pet.img);
    tuft_motion_step(&s_pet.tuft,lv_tick_get(),x,y);
    /* ---- 呆毛动感：蹦跳甩动 + 随机一阵风 ---- */
    static uint32_t wind_next=0, wind_started=0;
    static int wind_dir=1;
    uint32_t now=lv_tick_get();
    if(now>=wind_next) { wind_started=now; wind_next=now+lv_rand(8000,15000); wind_dir=lv_rand(0,1)?1:-1; }
    float drive=0;
    /* 蹦跳开心：呆毛大幅甩动 */
    if(s_pet.happy) drive += 1.1f * sinf(6.2831853f * (now%600) / 600.0f);
    /* 随机一阵风：900ms 正弦包络吹歪再弹回 */
    uint32_t wind_t=now-wind_started;
    if(wind_t<900) drive += wind_dir * 1.6f * sinf(3.1415927f * wind_t / 900.0f);
    if(drive!=0) {
        s_pet.tuft.bend=fmaxf(-2.4f,fminf(2.4f,s_pet.tuft.bend+drive));
        s_pet.tuft.velocity*=0.5f; /* 外力后衰减速度，让弹簧回弹自然 */
    }
    const lv_image_dsc_t *source=lv_img_get_src(s_pet.img);
    lv_img_set_src(s_pet.img,tuft_apply(source,s_pet.tuft.bend));
    if(s_pet.tuft.bend!=0) lv_obj_invalidate(s_pet.img);
}

/* Preview workbench: freeze daily behavior; only the selected demo action drives. */
static bool demo_mode = false;
/* Isolated layer for AI keyframes, sibling of the legacy character body. */
static lv_obj_t *s_demo_img = NULL;
static const lv_image_dsc_t * const ai_seq[];

/* Click dispatches one owned timeline; no independent click-bounce animation. */
static void happy_progress_cb(void *var, int32_t value);
static void happy_finish(void)
{
    if(!s_pet.happy) return;
    uint32_t dt=lv_tick_elaps(s_pet.happy_started);
    s_pet.started+=dt;s_pet.blink_started+=dt;s_pet.mouth_started+=dt;
    s_pet.head_returned+=dt;
    s_pet.happy=false;
    s_pet.happy_smiling=false;
    lv_anim_delete(s_pet.jump,happy_progress_cb);
    s_pet.blink_stage=s_pet.happy_blink_stage;
    lv_obj_set_style_transform_scale_x(s_pet.pet,256,0);
    lv_obj_set_style_transform_scale_y(s_pet.pet,256,0);
    lv_obj_set_style_translate_y(s_pet.jump,0,0);
    render_eyes();
    if(demo_mode) {
        /* Hold the sequence's own first frame through the rest gap; legacy
         * body stays hidden, so only AI frames are ever shown. */
        lv_img_set_src(s_demo_img, ai_seq[0]);
        lv_obj_set_style_translate_y(s_demo_img,0,0);
        lv_obj_invalidate(s_demo_img);
    }
    if(s_pet.happy_timer_paused) lv_timer_pause(s_pet.timer);

}

/* AI keyframe slots for the simulator workbench; pose squash is baked into
 * the images, so the demo only adds the vertical hop, not a second squash. */
static const lv_image_dsc_t * const happy_frames[] = {
    &pet_happy_0, &pet_happy_1, &pet_happy_2, &pet_happy_3, &pet_happy_4, &pet_happy_5
};

/* AI smooth sequence: jump height and smile transition are baked into each
 * frame, so the demo must NOT add its own vertical translate (would double-hop). */
#define AI_SEQ_N    30
#define AI_SEQ_MS   5000u
static const lv_image_dsc_t * const ai_seq[] = {
    &ai_happy_0, &ai_happy_1, &ai_happy_2, &ai_happy_3, &ai_happy_4,
    &ai_happy_5, &ai_happy_6, &ai_happy_7, &ai_happy_8, &ai_happy_9,
    &ai_happy_10, &ai_happy_11, &ai_happy_12, &ai_happy_13, &ai_happy_14,
    &ai_happy_15, &ai_happy_16, &ai_happy_17, &ai_happy_18, &ai_happy_19,
    &ai_happy_20, &ai_happy_21, &ai_happy_22, &ai_happy_23, &ai_happy_24,
    &ai_happy_25, &ai_happy_26, &ai_happy_27, &ai_happy_28, &ai_happy_29
};
static int happy_last_frame = -1;
static int happy_frame_for_t(uint32_t t)
{
    /* Prep -> takeoff -> peak -> landing -> reset; the F5 small-hop slot (4)
     * is intentionally skipped: no second bounce after the main jump. */
    if(t < 150)  return 0;
    if(t < 250)  return 1;
    if(t < 700)  return 2;
    if(t < 950)  return 3;
    return 5;
}

static void happy_progress_cb(void *var, int32_t value)
{
    (void)var;
    if(!s_pet.happy) return;
    uint32_t t=(uint32_t)value;
    uint32_t run_ms = demo_mode ? AI_SEQ_MS : 1600u;
    if(t>=run_ms) { happy_finish(); return; }

    if(demo_mode) {
        /* AI smooth sequence: pick frame by time; jump height and smile are
         * baked in, so no programmatic translate_y here (avoids double-hop). */
        uint32_t ft = t>=AI_SEQ_MS ? AI_SEQ_MS-1 : t;
        int idx = (int)(ft * AI_SEQ_N / AI_SEQ_MS);
        if(idx!=happy_last_frame) {
            happy_last_frame=idx;
            lv_img_set_src(s_demo_img, ai_seq[idx]);
            lv_obj_invalidate(s_demo_img);
        }
        return;
    }

    /* Hold a happy face through both hops and landing, then open the eyes. */
    s_pet.happy_smiling=t>=150 && t<1400;
    s_pet.blink_stage=t<40 ? 1 : t<100 ? 2 : t<150 ? 1 : 0;
    /* Foot-anchored anticipation, takeoff, contact squash and smaller echo.
     * Smooth interpolation makes every contact continuous instead of teleporting up. */
    static const struct { uint16_t ms; int16_t y, sx, sy; } keys[] = {
        {0,0,256,256}, {100,2,264,246}, {180,0,250,264},
        {380,-22,253,260}, {650,-7,256,256}, {760,0,265,246},
        {850,1,260,252}, {940,-5,252,261}, {1030,-9,255,258},
        {1190,0,261,250}, {1300,1,258,253}, {1460,0,256,256},
        {1600,0,256,256}
    };
    unsigned k=0;
    while(k+1<sizeof(keys)/sizeof(keys[0])-1 && t>keys[k+1].ms) k++;
    float u=(t-keys[k].ms)/(float)(keys[k+1].ms-keys[k].ms);
    u=u*u*(3.0f-2.0f*u);
    int y=(int)lroundf(keys[k].y+(keys[k+1].y-keys[k].y)*u);
    int sx=(int)lroundf(keys[k].sx+(keys[k+1].sx-keys[k].sx)*u);
    int sy=(int)lroundf(keys[k].sy+(keys[k+1].sy-keys[k].sy)*u);
    lv_obj_set_style_translate_y(s_pet.jump,y,0);
    lv_obj_set_style_transform_scale_x(s_pet.pet,sx,0);
    lv_obj_set_style_transform_scale_y(s_pet.pet,sy,0);
    render_eyes();
}

/* Preview workbench: when on, daily phase/walk/blink/mouth are frozen;
 * only the selected demo action (e.g. happy) drives the character. */

/* Shared start of the 1600ms happy timeline; callers own their own guards. */
static bool begin_happy(void)
{
    lv_obj_set_style_transform_pivot_x(s_pet.pet,96,0);
    lv_obj_set_style_transform_pivot_y(s_pet.pet,170,0);
    s_pet.happy_timer_paused=lv_timer_get_paused(s_pet.timer);
    s_pet.happy_blink_stage=s_pet.blink_stage;
    s_pet.happy=true;
    s_pet.happy_last=s_pet.happy_started=lv_tick_get();
    if(demo_mode) {
        happy_last_frame=-1;
        lv_obj_set_hidden(s_pet.pet,true);       /* hide legacy body entirely */
        lv_obj_set_hidden(s_demo_img,false);
        lv_img_set_src(s_demo_img, ai_seq[0]);
        lv_obj_set_style_translate_y(s_demo_img,0,0);
        lv_obj_invalidate(s_demo_img);
    }
    if(!demo_mode) lv_timer_resume(s_pet.timer);
    uint32_t run_ms = demo_mode ? AI_SEQ_MS : 1600u;
    lv_anim_t response;
    lv_anim_init(&response);
    lv_anim_set_var(&response,s_pet.jump);
    lv_anim_set_exec_cb(&response,happy_progress_cb);
    lv_anim_set_values(&response,0,(int32_t)run_ms);
    lv_anim_set_duration(&response,run_ms);
    lv_anim_set_path_cb(&response,lv_anim_path_linear);
    lv_anim_start(&response);
    return true;
}

bool pet_play_happy_once(void)
{
    if(!s_pet.pet || sleep_phase!=SLEEP_NONE || !s_pet.awake || s_pet.happy) return false;
    if(s_pet.happy_used && lv_tick_elaps(s_pet.happy_last)<2500) return false;

    s_pet.happy_used=true;
    return begin_happy();
}

/* Simulator preview only: force-loop happy with a gap, bypassing the 2500ms
 * cooldown. Not wired into firmware; driven by main.c's --happy-demo timer. */
void pet_happy_demo_tick(uint32_t gap_ms)
{
    static bool armed;
    static uint32_t since;
    if(!armed){ armed=true; since=lv_tick_get(); }
    if(s_pet.happy) return;                 /* let the current run finish */
    if(lv_tick_elaps(since)<gap_ms) return; /* rest gap between runs */
    if(!s_pet.pet || sleep_phase!=SLEEP_NONE || !s_pet.awake) return;
    begin_happy();
    since=lv_tick_get();
}

/* Simulator preview only: loop the angry keyframe sequence with per-frame
 * holds and a rest gap. Programmatic hold only; no AI redraw between frames.
 * Not wired into firmware; driven by main.c's --angry-demo timer. */
static const lv_image_dsc_t * const angry_seq[] = {
    &ai_angry_0, &ai_angry_1, &ai_angry_2, &ai_angry_3, &ai_angry_4, &ai_angry_5
};
/* A1 起气 / A2 小喊 / A3 大怒 / S1 提脚 / S2 踩下 / A4 收势 (ms holds) */
static const uint16_t angry_hold_ms[] = {660, 660, 490, 320, 410, 660};
static int angry_i = -1;
static uint32_t angry_at = 0;

void pet_angry_demo_tick(uint32_t gap_ms)
{
    if(!s_demo_img) return;
    uint32_t now = lv_tick_get();
    if(angry_i < 0) {
        angry_i = 0;
        angry_at = now;
        lv_img_set_src(s_demo_img, angry_seq[0]);
        lv_obj_invalidate(s_demo_img);
        return;
    }
    uint32_t el = lv_tick_elaps(angry_at);
    uint32_t budget = (angry_i == 5) ? (angry_hold_ms[5] + gap_ms) : angry_hold_ms[angry_i];
    if(el < budget) return;
    angry_i = (angry_i == 5) ? 0 : angry_i + 1;
    angry_at = now;
    lv_img_set_src(s_demo_img, angry_seq[angry_i]);
    lv_obj_invalidate(s_demo_img);
}

static void pet_click_cb(lv_event_t *event)
{
    (void)event;
    pet_play_happy_once();
}

void pet_set_demo_mode(bool on)
{
    demo_mode = on;
    if(!s_pet.pet) return;
    if(on) {
        lv_timer_pause(s_pet.timer);
        lv_timer_pause(s_pet.blink_timer);
        lv_timer_pause(s_pet.mouth_timer);
        s_pet.happy = false;
        s_pet.happy_smiling = false;
        s_pet.x = 0;
        lv_obj_set_style_translate_x(s_pet.pet, 0, 0);
        lv_obj_set_style_translate_y(s_pet.jump, 0, 0);
        lv_obj_set_style_transform_scale_x(s_pet.pet, 256, 0);
        lv_obj_set_style_transform_scale_y(s_pet.pet, 256, 0);
        /* Loop only the AI sequence: show its own first frame on entry, never
         * the legacy base or a separate F1 rest frame (avoids a cut/pop). */
        lv_obj_set_hidden(s_pet.pet, true);
        lv_obj_set_hidden(s_demo_img, false);
        lv_img_set_src(s_demo_img, ai_seq[0]);
        lv_obj_set_style_translate_y(s_demo_img, 0, 0);
        lv_obj_invalidate(s_demo_img);
    } else {
        lv_timer_resume(s_pet.timer);
        lv_timer_resume(s_pet.blink_timer);
        lv_timer_resume(s_pet.mouth_timer);
    }
}

static void routine_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if(sleep_phase!=SLEEP_NONE) return;
    if(demo_mode) return; /* freeze daily behavior; AI frames are driven by happy_progress_cb */
    uint32_t elapsed = lv_tick_elaps(s_pet.started);
    if(s_pet.happy) { update_tuft(); return; }
    uint32_t progress = LV_MIN(elapsed, s_pet.duration);
    switch(s_pet.phase) {
        case REST:
            rest_head_tick(progress);
            front_wing_tick(progress);
            break;
        case STAND:
            front_wing_tick(progress);
            break;
        case SETTLE_OUT:
        case SETTLE_BACK: {
            show_wing_frame(wing_settle_frame(s_pet.wing_settle_start,progress),s_pet.direction);
            break;
        }
        case RISE:
            show_frame(progress < 200 ? 1 : progress < 400 ? 2 : 3, 1);
            break;
        case LOOK:
            show_frame(progress < 180 ? 4 : 5, s_pet.direction);
            break;
        case OUT:
        case BACK: {
            float total=LV_ABS(s_pet.target_x-s_pet.from_x);
            float travel;
            if(!s_pet.walk_pause) travel=walk_travel(progress,s_pet.duration,total);
            else if(progress<s_pet.walk_first_ms)
                travel=walk_travel(progress,s_pet.walk_first_ms,s_pet.walk_checkpoint);
            else if(progress<s_pet.walk_first_ms+s_pet.walk_pause_ms)
                travel=s_pet.walk_checkpoint;
            else {
                uint32_t after=progress-s_pet.walk_first_ms-s_pet.walk_pause_ms;
                travel=s_pet.walk_checkpoint+walk_travel(after,s_pet.duration-s_pet.walk_first_ms-s_pet.walk_pause_ms,total-s_pet.walk_checkpoint);
            }
            s_pet.x = s_pet.from_x + s_pet.direction * (int32_t)lroundf(travel);
            lv_obj_set_style_translate_x(s_pet.pet, s_pet.x, 0);
            show_walk(travel, s_pet.direction);
            break;
        }
        case TURN:
            if(progress < 140) show_frame(5, s_pet.direction);
            else if(progress < 280) show_frame(4, s_pet.direction);
            else if(progress < 560) show_frame(3, 1);
            else if(progress < 700) show_frame(4, -s_pet.direction);
            else show_frame(5, -s_pet.direction);
            break;
        case FACE:
            show_frame(progress < 180 ? 5 : progress < 360 ? 4 : 3, s_pet.direction);
            break;
        case SIT:
            show_frame(progress < 200 ? 2 : progress < 400 ? 1 : 0, 1);
            break;
        default: break;
    }
    if(elapsed >= s_pet.duration) {
        daily_phase_t next=s_pet.phase==SIT ? REST : s_pet.phase+1;
        if(s_pet.phase==STAND && !s_pet.will_walk) next=SIT;
        enter_phase(next);
    }
    update_tuft();
}


/* Sleep owns the portrait and timers until its quiet wake-up interval ends.
 * Move original head pixels by at most 2px, tapering to zero at the neck;
 * the lower body/feet stay byte-identical. No new pose assets. */
static void sleep_nod_cb(void *var, int32_t amount)
{
    (void)var;
    sleep_nod=amount;
    const lv_image_dsc_t *base=pet_color_image(frames[0][0]);
    if(amount==0) { lv_img_set_src(s_pet.img,base); render_eyes(); return; }
    sleep_head_image=*base;
    sleep_head_image.header.flags|=LV_IMAGE_FLAGS_MODIFIABLE;
    sleep_head_image.data=sleep_head_pixels;
    memcpy(sleep_head_pixels,base->data,base->data_size);
    for(int y=1;y<140;y++) for(int x=0;x<192;x++) {
        float weight=y<110 ? 1.0f : (140-y)/30.0f;
        float sy=y-2.0f*amount/256.0f*weight;
        if(sy<0) sy=0;
        int y0=(int)floorf(sy),y1=y0+1;
        float fraction=sy-y0;
        const uint8_t *a=base->data+y0*base->header.stride+x*4;
        const uint8_t *b=base->data+y1*base->header.stride+x*4;
        uint8_t *out=sleep_head_pixels+y*base->header.stride+x*4;
        float wa=a[3]*(1-fraction),wb=b[3]*fraction,total=wa+wb;
        for(int c=0;c<3;c++) out[c]=total>0 ? (uint8_t)lroundf((a[c]*wa+b[c]*wb)/total) : 0;
        out[3]=(uint8_t)lroundf(total);
    }
    lv_img_set_src(s_pet.img,&sleep_head_image);
    lv_obj_invalidate(s_pet.img);
    render_eyes();
}
static void sleep_nod_start(int from,int to,uint32_t duration)
{
    lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,s_pet.img);
    lv_anim_set_exec_cb(&a,sleep_nod_cb);lv_anim_set_values(&a,from,to);
    lv_anim_set_duration(&a,duration);lv_anim_set_path_cb(&a,lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}
static void sleep_breath_cb(void *var,int32_t value)
{
    lv_obj_set_style_transform_scale_y(var,value,0);
}
static void sleep_breath_stop(void)
{
    lv_anim_delete(s_pet.img,sleep_breath_cb);
    lv_obj_set_style_transform_scale_y(s_pet.img,256,0);
    lv_obj_set_style_translate_y(s_pet.pet,0,0);
}
static void sleep_loop_begin(void)
{
    lv_anim_delete(s_pet.img,sleep_nod_cb);
    /* Existing sleep art is an upright yawning pose. Reuse its closed eyes
     * on the nodded nest portrait so folded wings, beak and feet stay put. */
    sleep_nod_cb(s_pet.img,256);
    const lv_image_dsc_t *art=pet_color_image(&pet_chick_sleep);
    static const int bounds[2][4]={{110,71,18,9},{62,66,17,8}};
    for(unsigned eye=0;eye<2;eye++) {
        const eye_region_t *e=&daily_eyes[0].eyes[eye];
        uint32_t skin=palette_skin_color(e->x-2,e->y+e->h/2);
        for(int y=e->y+2;y<e->y+e->h+2;y++) for(int x=e->x;x<e->x+e->w;x++) {
            uint8_t *out=sleep_head_pixels+y*192*4+x*4;
            out[0]=skin&255;out[1]=(skin>>8)&255;out[2]=(skin>>16)&255;out[3]=255;
        }
        for(int y=0;y<7;y++) for(int x=0;x<e->w-4;x++) {
            int sx=bounds[eye][0]+x*bounds[eye][2]/(e->w-4);
            int sy=bounds[eye][1]+y*bounds[eye][3]/7;
            const uint8_t *a=art->data+sy*art->header.stride+sx*4;
            float opacity=fmaxf(0,fminf(1,(190.0f-a[1])/100.0f));
            uint8_t *out=sleep_head_pixels+(e->y+8+y)*192*4+(e->x+2+x)*4;
            for(int c=0;c<3;c++) out[c]=(uint8_t)lroundf(out[c]*(1-opacity)+a[c]*opacity);
        }
    }
    sleep_nod=0;
    sleep_phase=SLEEP_LOOP;s_pet.awake=false;
    lv_img_set_src(s_pet.img,&sleep_head_image);lv_obj_invalidate(s_pet.img);
    s_pet.eyes=NULL;s_pet.mouth=&daily_mouths[0];s_pet.mouth_stage=0;render_eyes();update_mouth_mask();
    lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,s_pet.img);
    lv_anim_set_exec_cb(&a,sleep_breath_cb);lv_anim_set_values(&a,256,254);
    lv_anim_set_duration(&a,2200);lv_anim_set_reverse_duration(&a,2200);
    lv_anim_set_path_cb(&a,lv_anim_path_ease_in_out);
    lv_anim_set_repeat_count(&a,LV_ANIM_REPEAT_INFINITE);lv_anim_start(&a);
    lv_timer_pause(sleep_timer);
    if(sleep_exit_pending) pet_exit_sleep();
}
pet_sleep_phase_t pet_get_sleep_phase(void) { return sleep_phase; }
void pet_enter_sleep(void)
{
    if(!s_pet.img || sleep_phase!=SLEEP_NONE) return;
    sleep_phase=SLEEP_ENTER;sleep_exit_pending=false;sleep_started=lv_tick_get();
    sleep_drowsy_started=false;sleep_nod=0;
    sleep_was_calling=s_pet.calling;sleep_call_elapsed=lv_tick_elaps(s_pet.mouth_started);
    sleep_head_start=s_pet.head_frame;
    lv_timer_pause(s_pet.timer);lv_timer_pause(s_pet.blink_timer);lv_timer_pause(s_pet.mouth_timer);
    happy_finish();
    set_breathing(false);s_pet.happy=false;
    lv_obj_set_style_translate_y(s_pet.jump,0,0);
    s_pet.calling=false;
    s_pet.gaze_offset=s_pet.gaze_y=0;s_pet.blinking=false;s_pet.blink_stage=0;
    sleep_was_walking=s_pet.phase==OUT || s_pet.phase==BACK;
    sleep_was_seated=s_pet.phase==REST;
    sleep_walk_ms=0;
    if(sleep_was_walking) {
        sleep_walk_from=LV_ABS(s_pet.x-s_pet.from_x);
        sleep_walk_to=ceilf(sleep_walk_from/(WALK_STRIDE_PX/2.0f))*(WALK_STRIDE_PX/2.0f);
        sleep_walk_direction=s_pet.direction;
        sleep_walk_ms=(uint32_t)ceilf((sleep_walk_to-sleep_walk_from)*WALK_CYCLE_MS/WALK_STRIDE_PX);
    }
    /* Walking: finish this step, settle, face forward, then sit. */
    sleep_prep_ms=sleep_was_walking ? sleep_walk_ms+180+540+600 : sleep_was_seated ? 300 : 300+540+600;
    lv_timer_resume(sleep_timer);lv_timer_reset(sleep_timer);
}
void pet_exit_sleep(void)
{
    if(sleep_phase==SLEEP_ENTER) { sleep_exit_pending=true;return; }
    if(sleep_phase!=SLEEP_LOOP) return;
    sleep_breath_stop();sleep_phase=SLEEP_EXIT;sleep_started=lv_tick_get();
    s_pet.awake=true;s_pet.blink_stage=2;show_frame(0,1);
    sleep_nod_cb(s_pet.img,256);sleep_nod_start(256,0,400);
    lv_timer_resume(sleep_timer);lv_timer_reset(sleep_timer);
}
static void sleep_tick(lv_timer_t *timer)
{
    (void)timer;uint32_t t=lv_tick_elaps(sleep_started);
    if(sleep_phase==SLEEP_ENTER) {
        if(t<sleep_prep_ms) {
            if(sleep_was_walking && t<sleep_walk_ms) {
                float distance=sleep_walk_from+(sleep_walk_to-sleep_walk_from)*t/sleep_walk_ms;
                s_pet.x=s_pet.from_x+sleep_walk_direction*(int32_t)lroundf(distance);
                lv_obj_set_style_translate_x(s_pet.pet,s_pet.x,0);show_walk(distance,sleep_walk_direction);
            } else if(sleep_was_walking && t<sleep_walk_ms+180) {
                s_pet.x=s_pet.from_x+sleep_walk_direction*(int32_t)lroundf(sleep_walk_to);
                lv_obj_set_style_translate_x(s_pet.pet,s_pet.x,0);
                show_walk(sleep_walk_to,sleep_walk_direction);
                show_wing_frame(wing_settle_frame(wing_walk_frame(sleep_walk_to),t-sleep_walk_ms),sleep_walk_direction);
            } else if(!sleep_was_walking && t<300) {
                if(s_pet.head_turning) {
                    unsigned f=t<100 ? LV_MIN(sleep_head_start,2) : t<200 ? LV_MIN(sleep_head_start,1) : 0;
                    show_rest_head(f,s_pet.head_direction);
                } else if(s_pet.front_flapping && s_pet.front_base) {
                    unsigned f=t<100 ? 4 : t<200 ? 5 : 0;
                    lv_img_set_src(s_pet.img,front_body_pose(s_pet.front_base,s_pet.front_seated,f));
                }
            } else {
                uint32_t q=t-(sleep_was_walking ? sleep_walk_ms+180 : 300);
                if(!sleep_was_seated && q<540) show_frame(q<180 ? 5 : q<360 ? 4 : 3,s_pet.direction);
                else if(!sleep_was_seated) {q-=540;show_frame(q<200 ? 2 : q<400 ? 1 : 0,1);}
            }
            if(sleep_was_calling && t<300) {
                uint32_t c=sleep_call_elapsed+t;
                s_pet.mouth_stage=c<100 ? 1 : c<240 ? 2 : c<340 ? 1 : 0;
                refresh_side_portrait();lv_obj_invalidate(s_pet.img);
            }
            return;
        }
        if(!sleep_drowsy_started) {
            sleep_drowsy_started=true;
            s_pet.head_turning=s_pet.front_flapping=false;s_pet.mouth_stage=0;
            memset(&s_pet.tuft,0,sizeof(s_pet.tuft));show_frame(0,1);
        }
        uint32_t q=t-sleep_prep_ms;
        s_pet.blink_stage=q<200 ? 0 : q<320 ? 1 : 2;render_eyes();
        if(q>=620 && sleep_nod==0 && !lv_anim_get(s_pet.img,sleep_nod_cb)) sleep_nod_start(0,256,600);
        if(q>=1500) sleep_loop_begin();
    } else if(sleep_phase==SLEEP_EXIT) {
        s_pet.blink_stage=t<600 ? 2 : t<700 ? 1 : 0;render_eyes();
        if(t>=1400) {
            sleep_phase=SLEEP_NONE;lv_timer_pause(sleep_timer);s_pet.blink_stage=0;
            s_pet.blink_started=lv_tick_get();s_pet.blink_wait=lv_rand(2000,5000);
            s_pet.mouth_started=lv_tick_get();s_pet.mouth_wait=lv_rand(10000,20000);
            enter_phase(REST);s_pet.head_wait=lv_rand(3000,4000);
            s_pet.front_flap_wait=LV_MAX(s_pet.front_flap_wait,3000);
            lv_timer_resume(s_pet.timer);lv_timer_resume(s_pet.blink_timer);lv_timer_resume(s_pet.mouth_timer);
        }
    }
}

void pet_set_state(pet_state_t state)
{
    if(s_pet.pet == NULL || s_pet.img == NULL) return;
    if(state == PET_STATE_SLEEP) { pet_enter_sleep(); return; }
    if(sleep_phase != SLEEP_NONE) {
        if(state == PET_STATE_IDLE) pet_exit_sleep();
        return;
    }
    happy_finish();
    set_breathing(false);
    s_pet.happy = false;
    lv_obj_set_style_translate_y(s_pet.jump, 0, 0);
    lv_obj_set_hidden(s_pet.wing,true);
    s_pet.front_flapping=false;
    lv_obj_set_pos(s_pet.img,0,0);
    lv_image_set_rotation(s_pet.img,0);
    lv_timer_pause(s_pet.timer);
    memset(&s_pet.tuft,0,sizeof(s_pet.tuft));
    s_pet.side_portrait=false;
    s_pet.head_turning=false;
    s_pet.gaze_offset=0;
    s_pet.gaze_y=0;
    s_pet.calling = false;
    s_pet.mouth_stage = 0;
    s_pet.mouth = NULL;
    for(unsigned foot = 0; foot < 2; foot++) lv_obj_set_hidden(s_pet.feet[foot], true);
    s_pet.awake = state != PET_STATE_SLEEP;
    switch(state) {
        case PET_STATE_HUNGRY:
            set_breathing(true);
            lv_img_set_src(s_pet.img,pet_color_image( &pet_chick_hungry));
            s_pet.eyes = &hungry_eyes;
            s_pet.mouth = NULL;
            s_pet.eye_direction = 1;
            render_eyes();
            break;
        case PET_STATE_SLEEP:
            lv_img_set_src(s_pet.img,pet_color_image( &pet_chick_sleep));
            s_pet.blinking = false;
            s_pet.blink_stage = 0;
            s_pet.calling = false;
            s_pet.mouth_stage = 0;
            s_pet.mouth = NULL;
            render_eyes();
            break;
        case PET_STATE_WALK:
            enter_phase(LOOK);
            lv_timer_resume(s_pet.timer);
            break;
        case PET_STATE_IDLE:
        default:
            /* Resume from the current location without teleporting. */
            enter_phase(s_pet.x == 0 ? REST : BACK);
            lv_timer_resume(s_pet.timer);
            break;
    }
    update_mouth_mask();
}

void pet_create(lv_obj_t *parent, lv_coord_t w, lv_coord_t h)
{
    lv_obj_set_scrollable(parent, false);
    scene_create_background(parent,w,h);

    s_pet.jump=lv_obj_create(parent);
    lv_obj_remove_style_all(s_pet.jump);
    lv_obj_set_size(s_pet.jump,w,h);
    lv_obj_align(s_pet.jump,LV_ALIGN_TOP_LEFT,0,0);
    lv_obj_set_scrollable(s_pet.jump,false);
    lv_obj_set_clickable(s_pet.jump,false);
    lv_obj_set_overflow_visible(s_pet.jump,true);
    lv_obj_t *pet = lv_obj_create(s_pet.jump);
    lv_obj_remove_style_all(pet);
    lv_obj_set_size(pet, 192, 192);
    lv_obj_align(pet, LV_ALIGN_CENTER, 0, 50);
    lv_obj_set_clickable(pet, true);
    lv_obj_set_scrollable(pet, false);
    lv_obj_add_event_cb(pet, pet_click_cb, LV_EVENT_CLICKED, NULL);
    s_pet.shadow=lv_obj_create(pet);
    lv_obj_remove_style_all(s_pet.shadow);
    lv_obj_set_style_radius(s_pet.shadow,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_bg_color(s_pet.shadow,lv_color_hex(0x486E32),0);
    lv_obj_set_style_bg_opa(s_pet.shadow,22,0);
    lv_obj_set_style_shadow_color(s_pet.shadow,lv_color_hex(0x486E32),0);
    lv_obj_set_style_shadow_width(s_pet.shadow,12,0);
    lv_obj_set_style_shadow_opa(s_pet.shadow,12,0);
    lv_obj_set_clickable(s_pet.shadow,false);
    lv_obj_set_scrollable(s_pet.shadow,false);
    /* Far foot behind near foot, both beneath the unchanged body artwork. */
    for(int foot = 1; foot >= 0; foot--) {
        s_pet.feet[foot] = lv_img_create(pet);
        lv_img_set_src(s_pet.feet[foot], pet_color_image(foot_images[0][foot]));
        lv_obj_set_clickable(s_pet.feet[foot], false);
        lv_obj_set_scrollable(s_pet.feet[foot], false);
    }
    lv_obj_t *img = lv_img_create(pet);
    lv_img_set_src(img, pet_color_image(frames[0][0]));
    lv_obj_set_size(img,192,192); /* Transparent wing child bounds must not move the torso. */
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_clickable(img, false);
    lv_obj_add_event_cb(pet, mouth_draw_cb, LV_EVENT_DRAW_POST_END, NULL);
    s_pet.wing=lv_img_create(img);
    lv_img_set_src(s_pet.wing,wing_animation[0][0]);
    lv_obj_set_clickable(s_pet.wing,false);
    lv_obj_set_scrollable(s_pet.wing,false);
    lv_obj_set_hidden(s_pet.wing,true);

    for(unsigned i = 0; i < 2; i++) {
        lv_obj_t *cover = lv_obj_create(img);
        lv_obj_remove_style_all(cover);
        lv_obj_set_style_bg_opa(cover, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(cover, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_clickable(cover, false);
        lv_obj_set_scrollable(cover, false);
        s_pet.eye_covers[i] = cover;
        lv_obj_t *pupil = lv_obj_create(cover);
        lv_obj_remove_style_all(pupil);
        lv_obj_set_style_bg_color(pupil, lv_color_hex(0x382113), 0);
        lv_obj_set_style_bg_opa(pupil, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(pupil, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_clickable(pupil, false);
        s_pet.eye_pupils[i] = pupil;
        lv_obj_t *line = lv_line_create(cover);
        lv_obj_set_pos(line, 0, 0);
        lv_obj_set_style_line_color(line, lv_color_hex(0x382113), 0);
        lv_obj_set_style_line_width(line, 2, 0);
        lv_obj_set_style_line_rounded(line, true, 0);
        lv_obj_set_clickable(line, false);
        s_pet.eye_lines[i] = line;
        s_pet.eye_glances[i]=lv_img_create(cover);
        lv_obj_set_clickable(s_pet.eye_glances[i],false);
        lv_obj_set_scrollable(s_pet.eye_glances[i],false);
        lv_obj_set_hidden(s_pet.eye_glances[i],true);
    }

    s_pet.pet = pet;
    s_pet.img = img;
    /* Isolated AI-sequence layer, same spot as the legacy body; hidden by default. */
    s_demo_img = lv_img_create(s_pet.jump);
    lv_obj_remove_style_all(s_demo_img);
    lv_obj_set_size(s_demo_img, 192, 192);
    lv_obj_align(s_demo_img, LV_ALIGN_CENTER, 0, 50);
    lv_obj_set_clickable(s_demo_img, false);
    lv_obj_set_scrollable(s_demo_img, false);
    lv_obj_set_hidden(s_demo_img, true);
    s_pet.x = 0;
    s_pet.timer = lv_timer_create(routine_timer_cb, 20, NULL);
    s_pet.blink_started = lv_tick_get();
    s_pet.blink_wait = lv_rand(2000, 5000);
    s_pet.blink_stage = 0;
    s_pet.blinking = false;
    s_pet.blink_timer = lv_timer_create(blink_timer_cb, 20, NULL);
    s_pet.mouth_started = lv_tick_get();
    s_pet.mouth_wait = lv_rand(10000, 20000);   /* 轻叫 10~20s（原 20~40s） */
    s_pet.mouth_stage = 0;
    s_pet.calling = false;
    s_pet.mouth_timer = lv_timer_create(mouth_timer_cb, 20, NULL);
    sleep_timer=lv_timer_create(sleep_tick,20,NULL);
    lv_timer_pause(sleep_timer);
    pet_set_state(PET_STATE_IDLE);
    scene_create_foreground(parent);
}
