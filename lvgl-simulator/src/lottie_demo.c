/* Lottie 小鸡动画 Demo：LVGL lv_lottie + ThorVG 播放，动画数据见 lottie_chick_flap_data.h */
#include "lvgl.h"
#include "lottie_chick_flap_data.h"

void lottie_demo_create(lv_obj_t *parent, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *obj = lv_lottie_create(parent);
    /* ThorVG 渲染需要手动分配 ARGB8888 buffer，尺寸 = 宽 × 高 × 4 */
    static uint8_t lottie_buf[192 * 192 * 4] __attribute__((aligned(8)));
    lv_lottie_set_buffer(obj, 192, 192, lottie_buf);
    lv_lottie_set_src_data(obj, chick_flap_lottie, chick_flap_lottie_len);
    lv_obj_align(obj, LV_ALIGN_CENTER, 0, 50);
    (void)w; (void)h;
}
