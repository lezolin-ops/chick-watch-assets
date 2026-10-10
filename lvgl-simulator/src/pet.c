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
    lv_timer_t *blink_timer;
    uint8_t eyelid;
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

/* Eyelids are native LVGL drawing, independent of body transforms.
 * Coordinates refer to the unchanged 192px front sprite only. */
static void eyelid_anim_cb(void *var, int32_t value)
{
    (void)var;
    s_pet.eyelid = (uint8_t)value;
    lv_obj_invalidate(s_pet.img);
}

static void schedule_blink(void)
{
    lv_timer_set_period(s_pet.blink_timer, lv_rand(2800, 5200));
    lv_timer_reset(s_pet.blink_timer);
}

static void blink_ready_cb(lv_anim_t *a)
{
    (void)a;
    eyelid_anim_cb(s_pet.img, 0);
    schedule_blink();
    if(s_pet.state == PET_STATE_IDLE) lv_timer_resume(s_pet.blink_timer);
}

static void blink_timer_cb(lv_timer_t *timer)
{
    if(s_pet.state != PET_STATE_IDLE) return;
    lv_timer_pause(timer);
    lv_anim_t blink;
    lv_anim_init(&blink);
    lv_anim_set_var(&blink, s_pet.img);
    lv_anim_set_exec_cb(&blink, eyelid_anim_cb);
    lv_anim_set_values(&blink, 0, 100);
    lv_anim_set_duration(&blink, 65);
    lv_anim_set_reverse_delay(&blink, 55);
    lv_anim_set_reverse_duration(&blink, 85);
    lv_anim_set_path_cb(&blink, lv_anim_path_ease_in_out);
    lv_anim_set_completed_cb(&blink, blink_ready_cb);
    lv_anim_start(&blink);
}

static void eye_draw_cb(lv_event_t *e)
{
    if(s_pet.eyelid == 0 || s_pet.state != PET_STATE_IDLE) return;
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t image_area;
    lv_obj_get_coords(s_pet.img, &image_area);
    /* Small eye regions avoid the eyebrows, cheeks and beak. */
    static const lv_area_t eyes[] = {{60, 64, 76, 82}, {112, 67, 127, 85}};
    for(unsigned eye = 0; eye < 2; eye++) {
        const lv_area_t *box = &eyes[eye];
        int32_t cover = (box->y2 - box->y1 + 1) * s_pet.eyelid / 100;
        for(int32_t y = box->y1; y < box->y1 + cover; y++) {
            /* Sample adjacent feather color from the immutable BGRA image. */
            int32_t sample_x = eye == 0 ? 79 : 109;
            const uint8_t *pixel = pet_chick_192.data + y * pet_chick_192.header.stride + sample_x * 4;
            lv_draw_rect_dsc_t lid;
            lv_draw_rect_dsc_init(&lid);
            lid.bg_color = lv_color_make(pixel[2], pixel[1], pixel[0]);
            lid.bg_opa = LV_OPA_COVER;
            lid.border_width = 0;
            lv_area_t row = {image_area.x1 + box->x1, image_area.y1 + y,
                             image_area.x1 + box->x2, image_area.y1 + y};
            lv_draw_rect(layer, &lid, &row);
        }
        if(s_pet.eyelid >= 90) {
            /* A shallow downward curve reads as relaxed closed eyes. */
            lv_draw_line_dsc_t line;
            lv_draw_line_dsc_init(&line);
            line.color = lv_color_hex(0x382416);
            line.width = 2;
            line.round_start = line.round_end = 1;
            int32_t x = image_area.x1 + box->x1 + 2;
            int32_t y = image_area.y1 + (box->y1 + box->y2) / 2;
            line.p1 = (lv_point_precise_t){x, y};
            line.p2 = (lv_point_precise_t){x + 5, y + 2};
            lv_draw_line(layer, &line);
            line.p1 = line.p2;
            line.p2 = (lv_point_precise_t){image_area.x1 + box->x2 - 2, y};
            lv_draw_line(layer, &line);
        }
    }
}

static void shadow_delete_cb(lv_event_t *e)
{
    (void)e;
    s_pet.shadow = NULL;
}

static void pet_delete_cb(lv_event_t *e)
{
    (void)e;
    lv_timer_delete(s_pet.blink_timer);
    lv_timer_delete(s_pet.walk_frame_timer);
    lv_anim_delete(s_pet.img, eyelid_anim_cb);
    if(s_pet.shadow != NULL) lv_obj_delete(s_pet.shadow);
    s_pet = (pet_runtime_t){0};
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

    lv_anim_delete(s_pet.img, eyelid_anim_cb);
    eyelid_anim_cb(s_pet.img, 0);
    lv_timer_pause(s_pet.blink_timer);
    schedule_blink();
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
            lv_timer_resume(s_pet.blink_timer);
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
    lv_obj_add_event_cb(shadow, shadow_delete_cb, LV_EVENT_DELETE, NULL);
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
    lv_obj_add_event_cb(img, eye_draw_cb, LV_EVENT_DRAW_MAIN_END, NULL);
    lv_obj_add_event_cb(pet, pet_delete_cb, LV_EVENT_DELETE, NULL);


    s_pet.pet = pet;
    s_pet.img = img;
    s_pet.walk_frame = 0;
    s_pet.walk_frame_timer = lv_timer_create(walk_frame_timer_cb, 150, &s_pet);
    lv_timer_pause(s_pet.walk_frame_timer);
    s_pet.eyelid = 0;
    s_pet.blink_timer = lv_timer_create(blink_timer_cb, 4000, &s_pet);


    /* Gentle breathing anchored at the feet; no idle vertical drift. */
    start_loop_anim(pet, scale_anim_cb, 256, 260, 1600, lv_anim_path_ease_in_out);
    pet_set_state(PET_STATE_IDLE);
}
