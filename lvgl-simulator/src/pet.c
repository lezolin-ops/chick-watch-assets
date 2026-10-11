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
    uint32_t happy_started;
} pet_runtime_t;

static pet_runtime_t s_pet;
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
    if(m==NULL || !s_pet.awake || m->count==0 || m->mode==2) {
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
    if(!s_pet.awake || m==NULL || m->count==0 || m->mode==2) return;
    lv_layer_t *layer=lv_event_get_layer(event);
    lv_area_t origin;
    lv_obj_get_coords(s_pet.img,&origin);
    int direction=s_pet.eye_direction<0 ? -1 : 1;
    lv_draw_rect_dsc_t fill;
    lv_draw_rect_dsc_init(&fill);
    fill.bg_opa=LV_OPA_COVER;
    for(unsigned i=0;i<m->count;i++) {
        const mouth_row_t *r=&m->rows[i];
        int last=LV_MIN(r->x2,r->skin_x2);
        if(last>=r->x1) {
            int x1=direction<0 ? 191-last : r->x1;
            int x2=direction<0 ? 191-r->x1 : last;
            fill.bg_color=lv_color_hex(palette_skin_color(m->x-2,r->y));
            lv_area_t a={origin.x1+x1,origin.y1+r->y,origin.x1+x2,origin.y1+r->y};
            lv_draw_rect(layer,&fill,&a);
        }
        if(m->mode==2 && r->skin_x2>=r->x1 && r->skin_x2<=r->x2) {
            int x=direction<0 ? 191-r->skin_x2 : r->skin_x2;
            fill.bg_color=lv_color_hex(0xE99D1B);
            lv_area_t a={origin.x1+x,origin.y1+r->y,origin.x1+x,origin.y1+r->y};
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
    if(!s_pet.awake || s_pet.mouth == NULL) return;
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

static void render_eyes(void)
{
    for(unsigned i = 0; i < 2; i++) {
        bool visible = s_pet.awake && (s_pet.blink_stage != 0 || s_pet.gaze_offset != 0 || s_pet.gaze_y != 0) && s_pet.eyes != NULL && i < s_pet.eyes->count;
        lv_obj_set_hidden(s_pet.eye_covers[i], !visible);
        if(!visible) continue;
        const eye_region_t *e = &s_pet.eyes->eyes[i];
        int x = s_pet.eye_direction < 0 ? 192 - e->x - e->w : e->x;
        bool glance=(s_pet.gaze_offset!=0 || s_pet.gaze_y!=0) && s_pet.blink_stage==0;
        lv_obj_set_pos(s_pet.eye_covers[i], x-(glance ? 2 : 0), e->y-(glance ? 2 : 0));
        lv_obj_set_size(s_pet.eye_covers[i], e->w+(glance ? 4 : 0), e->h+(glance ? 4 : 0));
        lv_obj_set_style_bg_color(s_pet.eye_covers[i],lv_color_hex(palette_skin_color(e->x-2,e->y+e->h/2)),0);
        bool closed = s_pet.blink_stage == 2;
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
                s_pet.eyelid_points[i][j].y = e->h / 2 + (j == 2 ? 2 : (j == 0 || j == 4 ? 0 : 1));
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
    if(!s_pet.awake) return;
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
    /* ---- 呆毛动感增强：呼吸微摆 + 蹦跳甩动 + 随机一阵风 ---- */
    static uint32_t wind_next=0, wind_started=0;
    static int wind_dir=1;
    uint32_t now=lv_tick_get();
    if(now>=wind_next) { wind_started=now; wind_next=now+lv_rand(8000,15000); wind_dir=lv_rand(0,1)?1:-1; }
    float drive=0;
    /* 呼吸：3.2s 周期，呆毛随呼吸轻轻前后摆 */
    drive += 0.30f * sinf(6.2831853f * (now%3200) / 3200.0f);
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

static void happy_tick(uint32_t elapsed)
{
    (void)elapsed;
    uint32_t t = lv_tick_elaps(s_pet.happy_started);
    /* 蹦跳：每 600ms 一跳，抛物线高度 20px */
    uint32_t hop = t % 600;
    float u = hop < 300 ? hop / 300.0f : (600 - hop) / 300.0f;
    int dy = -(int)(20.0f * u * u * u);
    lv_obj_set_style_translate_y(s_pet.pet, dy, 0);
    /* 扇翅：每 700ms 一轮完整挥翅（身体贴图内嵌翅膀） */
    if(!s_pet.front_base) { show_frame(0, 1); s_pet.front_base = pet_color_image(frames[0][0]); s_pet.front_seated = true; }
    uint32_t wf_t = t % 700;
    unsigned frame = wing_walk_frame(wf_t * (WALK_STRIDE_PX / 700.0f));
    lv_img_set_src(s_pet.img, front_body_pose(s_pet.front_base, s_pet.front_seated, frame));
    render_eyes(); update_mouth_mask();
    if(t >= 1500) {
        s_pet.happy = false;
        lv_obj_set_style_translate_y(s_pet.pet, 0, 0);
        enter_phase(s_pet.phase); /* 从当前相位重新开始，不跳状态机 */
    }
}

static void pet_click_cb(lv_event_t *event)
{
    (void)event;
    if(!s_pet.awake) return;          /* 睡觉不理会 */
    if(s_pet.happy) return;           /* 正在开心，忽略连点 */
    s_pet.happy = true;
    s_pet.happy_started = lv_tick_get();
    s_pet.front_flapping = false;
    s_pet.front_base = NULL;
}

static void routine_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    uint32_t elapsed = lv_tick_elaps(s_pet.started);
    if(s_pet.happy) { happy_tick(elapsed); update_tuft(); return; }
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

void pet_set_state(pet_state_t state)
{
    if(s_pet.pet == NULL || s_pet.img == NULL) return;
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

    lv_obj_t *pet = lv_obj_create(parent);
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
    pet_set_state(PET_STATE_IDLE);
    scene_create_foreground(parent);
}
