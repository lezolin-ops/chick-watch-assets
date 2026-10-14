#include "watch_home_clock.h"
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(watch_cartoon_68);
LV_FONT_DECLARE(watch_font_24);
static lv_obj_t *s_time, *s_date;
static char s_last_time[16], s_last_date[48];
void watch_home_clock_set_enabled(bool enabled)
{
    if(!s_time) return;
    lv_obj_set_hidden(s_time, !enabled);
    lv_obj_set_hidden(s_date, !enabled);
}
void watch_home_clock_update(const struct tm *t, bool valid)
{
    if(!s_time) return;
    char clock[16] = "--:--", date[48] = "";
    if(valid && t && t->tm_mon >= 0 && t->tm_mon < 12 && t->tm_wday >= 0 && t->tm_wday < 7) {
        static const char *days[] = {"日", "一", "二", "三", "四", "五", "六"};
        snprintf(clock, sizeof(clock), "%02d:%02d", t->tm_hour, t->tm_min);
        snprintf(date, sizeof(date), "%d月%d日 周%s", t->tm_mon + 1, t->tm_mday, days[t->tm_wday]);
    }
    if(strcmp(clock, s_last_time)) {
        lv_label_set_text(s_time, clock); snprintf(s_last_time, sizeof(s_last_time), "%s", clock);
    }
    if(strcmp(date, s_last_date)) {
        lv_label_set_text(s_date, date); snprintf(s_last_date, sizeof(s_last_date), "%s", date);
    }
}
void watch_home_clock_create(lv_obj_t *screen)
{
    s_time = lv_label_create(screen); s_date = lv_label_create(screen);
    lv_obj_t *labels[] = {s_time, s_date};
    for(unsigned i = 0; i < 2; i++) {
        lv_obj_set_style_text_color(labels[i], lv_color_hex(0xFFFDF5), 0);
        lv_obj_set_style_bg_opa(labels[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_shadow_width(labels[i], 0, 0);
        lv_obj_set_style_text_align(labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_clickable(labels[i], false);
        lv_obj_set_width(labels[i], 290);
        lv_label_set_long_mode(labels[i], LV_LABEL_LONG_MODE_CLIP);
    }
    lv_obj_set_style_text_font(s_time, &watch_cartoon_68, 0);
    lv_obj_set_style_text_font(s_date, &watch_font_24, 0);
    lv_obj_align(s_time, LV_ALIGN_TOP_MID, 0, 48);
    lv_obj_align(s_date, LV_ALIGN_TOP_MID, 0, 152);
    s_last_time[0] = s_last_date[0] = '\0';
    lv_label_set_text(s_date, "");
    watch_home_clock_update(NULL, false);
}
