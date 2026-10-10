/** Electronic pet: grounded idle and one bounded happy jump per tap. */
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
    lv_obj_t *shadow;
    int32_t ground_y;
    lv_timer_t *walk_frame_timer;
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


static void bounce_y_ready_cb(lv_anim_t *a);
static void pet_click_cb(lv_event_t *e);
static void walk_frame_timer_cb(lv_timer_t *t);


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
    lv_obj_set_y((lv_obj_t *)var, s_pet.ground_y + v);
    if(s_pet.shadow != NULL) {
        int32_t height = v < 0 ? -v : 0;
        lv_obj_set_style_transform_scale_x(s_pet.shadow, 256 - height * 3, 0);
        lv_obj_set_style_bg_opa(s_pet.shadow, 55 - height, 0);
    }
}

static void translate_x_anim_cb(void *var, int32_t v)
{
    lv_obj_set_style_translate_x((lv_obj_t *)var, v, 0);
    if(s_pet.shadow != NULL) lv_obj_set_style_translate_x(s_pet.shadow, v, 0);
}

/* Happy motion owns vertical movement until landing. Repeated taps are ignored. */
static void bounce_y_ready_cb(lv_anim_t *a)
{
    (void)a;
    pet_set_state(PET_STATE_IDLE);
}

/* Alpha-mask click filtering includes inverse breathing transforms. */
static void pet_click_cb(lv_event_t *e)
{
    if(s_pet.state == PET_STATE_HAPPY) return;
    lv_indev_t *indev = lv_event_get_indev(e);
    if(indev == NULL) return;
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    lv_obj_transform_point(s_pet.img, &point, LV_OBJ_POINT_TRANSFORM_FLAG_INVERSE_RECURSIVE);
    lv_area_t area;
    lv_obj_get_coords(s_pet.img, &area);
    int32_t x = point.x - area.x1;
    int32_t y = point.y - area.y1;
    const lv_image_dsc_t *src = lv_image_get_src(s_pet.img);
    if(x < 0 || y < 0 || x >= PET_SPRITE_SIZE || y >= PET_SPRITE_SIZE) return;
    if(src->data[y * src->header.stride + x * 4 + 3] <= 32) return;
    pet_set_state(PET_STATE_HAPPY);
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

void pet_set_state(pet_state_t state)
{
    if(s_pet.pet == NULL || s_pet.img == NULL) return;

    lv_anim_del(s_pet.pet, y_anim_cb);
    lv_anim_del(s_pet.pet, translate_x_anim_cb);
    lv_obj_set_style_translate_x(s_pet.pet, 0, 0);
    lv_obj_set_style_transform_scale_y(s_pet.img, 256, 0);
    lv_anim_del(s_pet.img, scale_anim_cb);
    lv_anim_del(s_pet.img, scale_y_anim_cb);
    lv_obj_set_style_transform_scale(s_pet.img, 256, 0);
    y_anim_cb(s_pet.pet, 0);
    if(s_pet.shadow != NULL) lv_obj_set_style_translate_x(s_pet.shadow, 0, 0);
    s_pet.state = state;

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
        case PET_STATE_HAPPY: {
            lv_timer_pause(s_pet.walk_frame_timer);
            lv_img_set_src(s_pet.img, &pet_chick_192);
            lv_anim_t jump;
            lv_anim_init(&jump);
            lv_anim_set_var(&jump, s_pet.pet);
            lv_anim_set_exec_cb(&jump, y_anim_cb);
            lv_anim_set_values(&jump, 0, -18);
            lv_anim_set_duration(&jump, 220);
            lv_anim_set_path_cb(&jump, lv_anim_path_ease_out);
            lv_anim_set_playback_duration(&jump, 280);
            lv_anim_set_ready_cb(&jump, bounce_y_ready_cb);
            lv_anim_start(&jump);
            break;
        }
        case PET_STATE_IDLE:
        default:
            lv_timer_pause(s_pet.walk_frame_timer);
            lv_img_set_src(s_pet.img, &pet_chick_192);
            /* Feet stay planted during idle breathing. */
            break;
    }
}

/* ---- 入口 ---- */
void pet_create(lv_obj_t *parent, lv_coord_t w, lv_coord_t h)
{


    /* 背景：深蓝渐变，营造手表屏夜间氛围 */
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x1B2440), 0);
    lv_obj_set_style_bg_grad_color(parent, lv_color_hex(0x0F1526), 0);
    lv_obj_set_style_bg_grad_dir(parent, LV_GRAD_DIR_VER, 0);

    /* Ground shadow is a sibling: it never jumps with the character. */
    lv_obj_t *shadow = lv_obj_create(parent);
    lv_obj_set_size(shadow, 110, 16);
    lv_obj_set_pos(shadow, (w - 110) / 2, (h - PET_SPRITE_SIZE) / 2 - 20 + 179);
    lv_obj_set_style_radius(shadow, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(shadow, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(shadow, 55, 0);
    lv_obj_set_style_border_width(shadow, 0, 0);
    lv_obj_set_style_transform_pivot_x(shadow, 55, 0);
    lv_obj_set_clickable(shadow, false);
    lv_obj_set_scrollable(shadow, false);
    s_pet.shadow = shadow;
    s_pet.ground_y = (h - PET_SPRITE_SIZE) / 2 - 20;

    /* 宠物主体容器：192×192，适配 410×502 AMOLED 显示区。 */
    lv_obj_t *pet = lv_obj_create(parent);
    lv_obj_set_size(pet, PET_SPRITE_SIZE, PET_SPRITE_SIZE);
    lv_obj_set_pos(pet, (w - PET_SPRITE_SIZE) / 2, s_pet.ground_y);
    lv_obj_set_style_pad_all(pet, 0, 0);
    lv_obj_set_clickable(pet, false);
    lv_obj_set_overflow_visible(pet, true);
    lv_obj_set_style_border_width(pet, 0, 0);
    lv_obj_set_style_bg_opa(pet, LV_OPA_TRANSP, 0);
    /* 宠物容器只负责布局和动画，禁用默认可滚动行为，避免缩放时出现滚动条。 */
    lv_obj_set_scrollable(pet, false);
    lv_obj_set_scrollbar_mode(pet, LV_SCROLLBAR_MODE_OFF);
    /* 缩放/压缩旋转中心设为容器中心 */
    lv_obj_set_style_transform_pivot_x(pet, PET_SPRITE_PIVOT, 0);
    lv_obj_set_style_transform_pivot_y(pet, 179, 0);

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


    /* Gentle breathing anchored at the feet; no idle vertical drift. */
    start_loop_anim(pet, scale_anim_cb, 256, 260, 1600, lv_anim_path_ease_in_out);
    pet_set_state(PET_STATE_IDLE);
}
