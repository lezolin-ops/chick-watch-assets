#include <assert.h>
#include <stdio.h>
#include "../../main/pet.c"

static unsigned renders;
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    (void)area; (void)pixels; renders++;
    lv_display_flush_ready(display);
}

int main(void)
{
    lv_init();
    static uint8_t pixels[410 * 40 * 4];
    lv_display_t *display = lv_display_create(410, 502);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display, pixels, NULL, sizeof(pixels), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    pet_create(lv_screen_active(), 410, 502);
    lv_refr_now(display);
    pet_set_paused(true);
    assert(lv_timer_get_paused(pet_scene.cloud_timer));
    assert(lv_timer_get_paused(pet_scene.wind_timer));
    assert(lv_timer_get_paused(s_pet.timer));
    uint32_t cloud_start = pet_scene.cloud_started;
    int cloud_x = lv_obj_get_x(pet_scene.clouds[0]);
    unsigned before = renders;
    for(unsigned i = 0; i < 100; i++) { lv_tick_inc(100); lv_timer_handler(); }
    assert(lv_obj_get_x(pet_scene.clouds[0]) == cloud_x);
    assert(renders == before);
    pet_set_paused(true); /* Repeated page transitions must not reset pause time. */
    pet_set_paused(false);
    assert(!lv_timer_get_paused(pet_scene.cloud_timer));
    assert(!lv_timer_get_paused(pet_scene.wind_timer));
    assert(pet_scene.cloud_started - cloud_start == 10000);
    uint32_t resumed = pet_scene.cloud_started;
    pet_set_paused(false);
    assert(pet_scene.cloud_started == resumed);
    lv_tick_inc(1000); lv_timer_handler();
    assert(lv_obj_get_x(pet_scene.clouds[0]) != cloud_x);
    puts("PASS: hidden scene stops computation/rendering, resumes without elapsed-time jump, repeated calls are safe");
    return 0;
}
