/**
 * @file pet.c
 * @brief 电子宠物核心模块实现（P0 动画 + 小黄鸡形象）
 *
 * 平台无关：仅依赖 LVGL v9，模拟器与真机共用。
 *
 * 形象：小黄鸡贴图（由项目中的透明 PNG 生成，192×192 规格，
 *       见 pet_chick_192.c）。
 *
 * P0 动画清单（来自《电子宠物动画系统技术实现清单.md》）：
 *  1. 呼吸动画：主体缩放循环插值（ease_in_out + 无限循环 + 双向播放）
 *  2. 心跳动画：定时触发的快速缩放脉冲（overshoot 回弹路径）
 *  3. 眨眼动画：贴图 Y 轴快速压缩回弹（模拟闭眼点头）
 *  4. 待机微动：主体 Y 轴轻微上下浮动（无限循环）
 *
 * 属性冲突规避：呼吸作用于 pet_group.scale、心跳作用于 img.scale、
 * 眨眼作用于 img.scale_y、微动作用于 pet_group.y，四者互不干扰。
 */

#include "pet.h"

/* 小黄鸡贴图数据（由 assets/runtime/chick/sprite_front_192.png 生成）。 */
#include "pet_chick_192.c"
#include "pet_chick_walk_0.c"
#include "pet_chick_walk_1.c"
#include "pet_chick_walk_2.c"
#include "pet_chick_walk_3.c"
#include "pet_chick_hungry.c"
#include "pet_chick_sleep.c"

#define PET_SPRITE_SIZE 192
#define PET_SPRITE_PIVOT (PET_SPRITE_SIZE / 2)

typedef struct {
    lv_obj_t *pet;
    lv_obj_t *img;
    lv_timer_t *walk_frame_timer;
    lv_timer_t *heartbeat_timer;
    lv_timer_t *blink_timer;
    pet_state_t state;
    uint8_t walk_frame;
} pet_runtime_t;

static pet_runtime_t s_pet;
static const lv_image_dsc_t * const walk_frames[] = {
    &pet_chick_walk_0, &pet_chick_walk_1,
    &pet_chick_walk_2, &pet_chick_walk_3,
};

/*********************
 *   STATIC PROTOTYPES
 *********************/
static void scale_anim_cb(void *var, int32_t v);
static void scale_y_anim_cb(void *var, int32_t v);
static void y_anim_cb(void *var, int32_t v);
static void translate_x_anim_cb(void *var, int32_t v);

static void start_heartbeat(lv_obj_t *img);
static void heartbeat_timer_cb(lv_timer_t *t);
static void start_blink(lv_obj_t *img);
static void blink_timer_cb(lv_timer_t *t);

static void bounce_y_ready_cb(lv_anim_t *a);
static void pet_click_cb(lv_event_t *e);
static void walk_frame_timer_cb(lv_timer_t *t);
static void routine_timer_cb(lv_timer_t *t);

static void start_loop_anim(lv_obj_t *obj, lv_anim_exec_xcb_t exec_cb,
                            int32_t from, int32_t to, uint32_t duration,
                            lv_anim_path_cb_t path_cb);

/*********************
 *   STATIC FUNCTIONS
 *********************/

/* LVGL v9 动画 exec 回调包装（统一签名 void (*)(void *, int32_t)） */
static void scale_anim_cb(void *var, int32_t v)
{
    lv_obj_set_style_transform_scale((lv_obj_t *)var, v, 0);
}

static void scale_y_anim_cb(void *var, int32_t v)
{
    lv_obj_set_style_transform_scale_y((lv_obj_t *)var, v, 0);
}

static void y_anim_cb(void *var, int32_t v)
{
    lv_obj_set_y((lv_obj_t *)var, v);
}

static void translate_x_anim_cb(void *var, int32_t v)
{
    lv_obj_set_style_translate_x((lv_obj_t *)var, v, 0);
}

/* ---- 心跳：定时触发的快速缩放脉冲（overshoot 回弹） ---- */
static void start_heartbeat(lv_obj_t *img)
{
    /* 清除 img 上残留的缩放动画，避免叠加 */
    lv_anim_del(img, scale_anim_cb);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, img);
    lv_anim_set_exec_cb(&a, scale_anim_cb);
    /* 缩放单位：256 = 100%；296 ≈ 115.6%（overshoot 带回弹，形成心跳冲击） */
    lv_anim_set_values(&a, 256, 296);
    lv_anim_set_duration(&a, 170);
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    lv_anim_set_playback_duration(&a, 170);
    lv_anim_start(&a);
}

static void heartbeat_timer_cb(lv_timer_t *t)
{
    start_heartbeat((lv_obj_t *)lv_timer_get_user_data(t));
}

/* ---- 眨眼：贴图 Y 轴快速压缩回弹（模拟闭眼点头） ---- */
static void start_blink(lv_obj_t *img)
{
    lv_anim_del(img, scale_y_anim_cb);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, img);
    lv_anim_set_exec_cb(&a, scale_y_anim_cb);
    /* 纵向压缩到约 92%，快速回弹，形成"眯眼"效果 */
    lv_anim_set_values(&a, 256, 236);
    lv_anim_set_duration(&a, 140);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_playback_duration(&a, 140);
    lv_anim_start(&a);
}

static void blink_timer_cb(lv_timer_t *t)
{
    lv_obj_t *img = (lv_obj_t *)lv_timer_get_user_data(t);
    start_blink(img);
    /* 下一次眨眼间隔随机化（1.8s ~ 4.2s），模拟自然眨眼 */
    lv_timer_set_period(t, lv_rand(1800, 4200));
}

/* ---- P1 交互：点击开心弹跳 ----
 * 点击后：身体放大回弹（bounce）+ 整只向上跳（bounce），
 * 跳完自动恢复待机微动（bounce_y_ready_cb 重启微动循环）。
 */
static void bounce_y_ready_cb(lv_anim_t *a)
{
    lv_obj_t *pet = (lv_obj_t *)a->var;
    start_loop_anim(pet, y_anim_cb, 0, -12, 1200, lv_anim_path_ease_in_out);
}

static void pet_click_cb(lv_event_t *e)
{
    lv_obj_t *img = lv_event_get_target(e);
    lv_obj_t *pet = (lv_obj_t *)lv_event_get_user_data(e);

    pet_set_state(PET_STATE_HAPPY);

    /* 开心放大弹跳（作用于贴图，不干扰呼吸动画） */
    lv_anim_del(img, scale_anim_cb);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, img);
    lv_anim_set_exec_cb(&a, scale_anim_cb);
    lv_anim_set_values(&a, 256, 320);          /* 320 ≈ 125% */
    lv_anim_set_duration(&a, 350);
    lv_anim_set_path_cb(&a, lv_anim_path_bounce);
    lv_anim_set_playback_duration(&a, 350);
    lv_anim_start(&a);

    /* 整只向上跳（先停掉待机微动，跳完恢复） */
    lv_anim_del(pet, y_anim_cb);
    lv_anim_t b;
    lv_anim_init(&b);
    lv_anim_set_var(&b, pet);
    lv_anim_set_exec_cb(&b, y_anim_cb);
    lv_anim_set_values(&b, 0, -40);
    lv_anim_set_duration(&b, 280);
    lv_anim_set_path_cb(&b, lv_anim_path_bounce);
    lv_anim_set_playback_duration(&b, 280);
    lv_anim_set_ready_cb(&b, bounce_y_ready_cb);
    lv_anim_start(&b);
}

/* ---- 无限循环动画通用启动器 ---- */
static void start_loop_anim(lv_obj_t *obj, lv_anim_exec_xcb_t exec_cb,
                            int32_t from, int32_t to, uint32_t duration,
                            lv_anim_path_cb_t path_cb)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, exec_cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, duration);
    lv_anim_set_path_cb(&a, path_cb);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_playback_duration(&a, duration);
    lv_anim_start(&a);
}

static void walk_frame_timer_cb(lv_timer_t *t)
{
    pet_runtime_t *runtime = lv_timer_get_user_data(t);
    runtime->walk_frame = (runtime->walk_frame + 1) % (sizeof(walk_frames) / sizeof(walk_frames[0]));
    lv_img_set_src(runtime->img, walk_frames[runtime->walk_frame]);
}

static void routine_timer_cb(lv_timer_t *t)
{
    (void)t;
    switch(s_pet.state) {
        case PET_STATE_IDLE:   pet_set_state(PET_STATE_WALK); break;
        case PET_STATE_WALK:   pet_set_state(PET_STATE_HUNGRY); break;
        case PET_STATE_HUNGRY: pet_set_state(PET_STATE_SLEEP); break;
        default:               pet_set_state(PET_STATE_IDLE); break;
    }
}

void pet_set_state(pet_state_t state)
{
    if(s_pet.pet == NULL || s_pet.img == NULL) return;

    lv_anim_del(s_pet.pet, y_anim_cb);
    lv_anim_del(s_pet.pet, translate_x_anim_cb);
    lv_obj_set_style_translate_x(s_pet.pet, 0, 0);
    lv_obj_set_style_transform_scale_y(s_pet.img, 256, 0);
    s_pet.state = state;

    if(s_pet.heartbeat_timer != NULL && s_pet.blink_timer != NULL) {
        if(state == PET_STATE_HUNGRY || state == PET_STATE_SLEEP) {
            lv_timer_pause(s_pet.heartbeat_timer);
            lv_timer_pause(s_pet.blink_timer);
        }
        else {
            lv_timer_resume(s_pet.heartbeat_timer);
            lv_timer_resume(s_pet.blink_timer);
        }
    }

    switch(state) {
        case PET_STATE_WALK:
            s_pet.walk_frame = 0;
            lv_img_set_src(s_pet.img, walk_frames[s_pet.walk_frame]);
            lv_timer_resume(s_pet.walk_frame_timer);
            start_loop_anim(s_pet.pet, y_anim_cb, 0, -8, 180, lv_anim_path_ease_in_out);
            start_loop_anim(s_pet.pet, translate_x_anim_cb, -52, 52, 1800, lv_anim_path_ease_in_out);
            break;
        case PET_STATE_HUNGRY:
            lv_timer_pause(s_pet.walk_frame_timer);
            lv_img_set_src(s_pet.img, &pet_chick_hungry);
            start_loop_anim(s_pet.pet, y_anim_cb, 0, 7, 950, lv_anim_path_ease_in_out);
            break;
        case PET_STATE_SLEEP:
            lv_timer_pause(s_pet.walk_frame_timer);
            lv_img_set_src(s_pet.img, &pet_chick_sleep);
            start_loop_anim(s_pet.pet, y_anim_cb, 0, -5, 1300, lv_anim_path_ease_in_out);
            break;
        case PET_STATE_HAPPY:
            lv_timer_pause(s_pet.walk_frame_timer);
            lv_img_set_src(s_pet.img, &pet_chick_192);
            start_loop_anim(s_pet.pet, y_anim_cb, 0, -10, 450, lv_anim_path_ease_in_out);
            break;
        case PET_STATE_IDLE:
        default:
            lv_timer_pause(s_pet.walk_frame_timer);
            lv_img_set_src(s_pet.img, &pet_chick_192);
            start_loop_anim(s_pet.pet, y_anim_cb, 0, -12, 1200, lv_anim_path_ease_in_out);
            break;
    }
}

/* ---- 入口 ---- */
void pet_create(lv_obj_t *parent, lv_coord_t w, lv_coord_t h)
{
    (void)w;
    (void)h;

    /* 背景：深蓝渐变，营造手表屏夜间氛围 */
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x1B2440), 0);
    lv_obj_set_style_bg_grad_color(parent, lv_color_hex(0x0F1526), 0);
    lv_obj_set_style_bg_grad_dir(parent, LV_GRAD_DIR_VER, 0);

    /* 宠物主体容器：192×192，适配 410×502 AMOLED 显示区。 */
    lv_obj_t *pet = lv_obj_create(parent);
    lv_obj_set_size(pet, PET_SPRITE_SIZE, PET_SPRITE_SIZE);
    lv_obj_align(pet, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_border_width(pet, 0, 0);
    lv_obj_set_style_bg_opa(pet, LV_OPA_TRANSP, 0);
    /* 宠物容器只负责布局和动画，禁用默认可滚动行为，避免缩放时出现滚动条。 */
    lv_obj_set_scrollable(pet, false);
    lv_obj_set_scrollbar_mode(pet, LV_SCROLLBAR_MODE_OFF);
    /* 缩放/压缩旋转中心设为容器中心 */
    lv_obj_set_style_transform_pivot_x(pet, PET_SPRITE_PIVOT, 0);
    lv_obj_set_style_transform_pivot_y(pet, PET_SPRITE_PIVOT, 0);

    /* 小黄鸡贴图 */
    lv_obj_t *img = lv_img_create(pet);
    lv_img_set_src(img, &pet_chick_192);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_transform_pivot_x(img, PET_SPRITE_PIVOT, 0);
    lv_obj_set_style_transform_pivot_y(img, PET_SPRITE_PIVOT, 0);
    /* 可点击，触发开心弹跳 */
    lv_obj_set_clickable(img, true);
    lv_obj_add_event_cb(img, pet_click_cb, LV_EVENT_CLICKED, pet);

    s_pet.pet = pet;
    s_pet.img = img;
    s_pet.walk_frame = 0;
    s_pet.walk_frame_timer = lv_timer_create(walk_frame_timer_cb, 150, &s_pet);
    lv_timer_pause(s_pet.walk_frame_timer);
    lv_timer_create(routine_timer_cb, 5000, NULL);

    /* ---- 状态共用动画 ---- */

    /* 1. 呼吸动画：主体缩放 100% ↔ 105%，3s 周期，无限循环双向播放 */
    start_loop_anim(pet, scale_anim_cb, 256, 269, 1500, lv_anim_path_ease_in_out);

    /* 2. 初始待机：微动，之后按 待机→走路→饥饿→睡觉 自动轮换。 */
    pet_set_state(PET_STATE_IDLE);

    /* 3. 心跳：定时脉冲（1.4s 一次） */
    s_pet.heartbeat_timer = lv_timer_create(heartbeat_timer_cb, 1400, img);

    /* 4. 眨眼：随机间隔（1.8s~4.2s） */
    s_pet.blink_timer = lv_timer_create(blink_timer_cb, lv_rand(1800, 4200), img);
}
