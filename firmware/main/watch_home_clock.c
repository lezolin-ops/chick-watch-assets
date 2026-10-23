#include "watch_home_clock.h"
#include "watch_steps.h"
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(watch_cartoon_68);
LV_FONT_DECLARE(watch_font_24);
LV_FONT_DECLARE(watch_steps_font_24);
LV_FONT_DECLARE(watch_step_numbers_30);
LV_FONT_DECLARE(watch_pill_numbers_24);
LV_FONT_DECLARE(watch_date_26);
static lv_obj_t *s_time, *s_date, *s_steps, *s_home_power;
static lv_obj_t *s_face, *s_face_time, *s_face_row, *s_face_date, *s_face_power;
static lv_obj_t *s_pill, *s_pill_text, *s_pill_unit;
static bool s_home_enabled=true;
static char s_battery[64]=LV_SYMBOL_BATTERY_EMPTY " --%";
static void text_if_changed(lv_obj_t *label,const char *text)
{
    if(label && strcmp(lv_label_get_text(label),text))lv_label_set_text(label,text);
}
static lv_obj_t *status_label(lv_obj_t *parent,const lv_font_t *font,uint32_t color)
{
    lv_obj_t *label=lv_label_create(parent);
    lv_obj_set_style_text_font(label,font,0);
    lv_obj_set_style_text_color(label,lv_color_hex(color),0);
    lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_bg_opa(label,LV_OPA_TRANSP,0);
    lv_obj_set_clickable(label,false);
    return label;
}
static void home_row_layout(void)
{
    if(!s_date)return;
    lv_obj_update_layout(lv_obj_get_parent(s_date));
    lv_point_t date_size,step_size;
    lv_text_get_size(&date_size,lv_label_get_text(s_date),&watch_date_26,0,0,1000,0);
    lv_text_get_size(&step_size,lv_label_get_text(s_steps),&watch_step_numbers_30,0,0,1000,0);
    int dw=s_home_enabled?date_size.x:0;
    int sw=step_size.x,gap=dw?20:0;
    /* Full UINT32 numbers and failure text remain visible, with a second
     * row when needed. No abbreviation or shrinking of the touch area. */
    if(dw+gap+sw<=362) {
        int left=(410-dw-gap-sw)/2;
        lv_obj_set_align(s_date,LV_ALIGN_TOP_LEFT);lv_obj_set_align(s_steps,LV_ALIGN_TOP_LEFT);
        lv_obj_set_pos(s_date,left,166);
        lv_obj_set_pos(s_steps,left+dw+gap,166);
    } else {
        lv_obj_align(s_date,LV_ALIGN_TOP_MID,0,170);
        lv_obj_align(s_steps,LV_ALIGN_TOP_MID,0,202);
    }
}
void watch_home_clock_set_enabled(bool enabled)
{
    s_home_enabled=enabled;
    if(!s_time)return;
    lv_obj_set_hidden(s_time,!enabled);lv_obj_set_hidden(s_date,!enabled);
    lv_obj_set_hidden(s_steps,!enabled);
    home_row_layout();
}
void watch_status_set_power(const char *text)
{
    snprintf(s_battery,sizeof(s_battery),"%s",text);
    text_if_changed(s_home_power,s_battery);
    text_if_changed(s_face_power,s_battery);
}
void watch_home_clock_update(const struct tm *t,bool valid)
{
    if(!s_time)return;
    uint32_t steps=0;bool failed=false;
    bool available=watch_steps_get(&steps,&failed);
    char value[80];
    if(available)snprintf(value,sizeof(value),"%lu 步%s",(unsigned long)steps,failed?"\n保存失败":"");
    else snprintf(value,sizeof(value),"计步暂不可用");
    text_if_changed(s_steps,value);
    char clock[16]="--:--",date[48]="";
    if(valid&&t&&t->tm_mon>=0&&t->tm_mon<12&&t->tm_wday>=0&&t->tm_wday<7) {
        static const char *days[]={"日","一","二","三","四","五","六"};
        snprintf(clock,sizeof(clock),"%02d:%02d",t->tm_hour,t->tm_min);
        snprintf(date,sizeof(date),"%d月%d日 周%s",t->tm_mon+1,t->tm_mday,days[t->tm_wday]);
    }
    text_if_changed(s_time,clock);text_if_changed(s_date,date);
    home_row_layout();
}
void watch_home_clock_create(lv_obj_t *screen)
{
    s_time=status_label(screen,&watch_cartoon_68,0xFFFDF5);
    lv_obj_set_width(s_time,290);lv_obj_align(s_time,LV_ALIGN_TOP_MID,0,60);
    s_date=status_label(screen,&watch_date_26,0xFFFDF5);
    s_steps=status_label(screen,&watch_step_numbers_30,0xFFFDF5);
    s_home_power=status_label(screen,&lv_font_montserrat_22,0xFFFDF5);
    lv_obj_set_width(s_home_power,290);lv_obj_align(s_home_power,LV_ALIGN_TOP_MID,0,28);
    lv_obj_set_width(s_steps,LV_SIZE_CONTENT);lv_obj_set_width(s_date,LV_SIZE_CONTENT);
    text_if_changed(s_home_power,s_battery);
    watch_home_clock_update(NULL,false);watch_home_clock_set_enabled(s_home_enabled);
}
void watch_standby_face_create(lv_obj_t *screen)
{
    s_face=lv_obj_create(screen);lv_obj_remove_style_all(s_face);
    lv_obj_set_size(s_face,362,280);lv_obj_align(s_face,LV_ALIGN_CENTER,0,0);
    lv_obj_set_scrollable(s_face,false);lv_obj_set_clickable(s_face,false);
    s_face_time=status_label(s_face,&lv_font_montserrat_48,0xDADADA);
    lv_obj_set_style_transform_scale(s_face_time,464,0);
    lv_obj_set_style_transform_pivot_x(s_face_time,lv_pct(50),0);
    lv_obj_set_style_transform_pivot_y(s_face_time,lv_pct(50),0);
    s_face_row=lv_obj_create(s_face);lv_obj_remove_style_all(s_face_row);
    lv_obj_set_size(s_face_row,LV_SIZE_CONTENT,LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_face_row,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_face_row,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_face_row,24,0);
    lv_obj_set_scrollable(s_face_row,false);lv_obj_set_clickable(s_face_row,false);
    s_face_date=status_label(s_face_row,&lv_font_montserrat_24,0xBBBBBB);
    s_face_power=status_label(s_face_row,&lv_font_montserrat_24,0xBDBDBD);
    s_pill=lv_obj_create(s_face);lv_obj_remove_style_all(s_pill);
    lv_obj_set_size(s_pill,LV_SIZE_CONTENT,LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(s_pill,lv_color_hex(0xE8E8E8),0);
    lv_obj_set_style_bg_opa(s_pill,LV_OPA_COVER,0);
    lv_obj_set_style_radius(s_pill,40,0);lv_obj_set_style_pad_hor(s_pill,20,0);lv_obj_set_style_pad_ver(s_pill,4,0);
    lv_obj_set_scrollable(s_pill,false);lv_obj_set_clickable(s_pill,false);
    lv_obj_set_flex_flow(s_pill,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_pill,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(s_pill,6,0);
    s_pill_text=status_label(s_pill,&watch_pill_numbers_24,0x111111);
    s_pill_unit=status_label(s_pill,&lv_font_montserrat_18,0x111111);
    lv_label_set_text(s_pill_unit,"STEP");lv_obj_set_style_translate_y(s_pill_unit,4,0);
    text_if_changed(s_face_power,s_battery);
    watch_standby_face_update(NULL,false);
}
void watch_standby_face_update(const struct tm *t,bool valid)
{
    if(!s_face)return;
    char clock[16]="--:--",date[40]="-- / --";
    int dx=0,dy=0;
    if(valid&&t&&t->tm_mon>=0&&t->tm_mon<12&&t->tm_wday>=0&&t->tm_wday<7) {
        static const char *days[]={"SUN","MON","TUE","WED","THU","FRI","SAT"};
        snprintf(clock,sizeof(clock),"%02d:%02d",t->tm_hour,t->tm_min);
        snprintf(date,sizeof(date),"%02d/%02d  %s",t->tm_mon+1,t->tm_mday,days[t->tm_wday]);
        dx=(t->tm_min%3-1)*4;dy=((t->tm_min/3)%3-1)*4;
    }
    text_if_changed(s_face_time,clock);text_if_changed(s_face_date,date);
    uint32_t steps=0;bool failed=false;char value[80];
    bool available=watch_steps_get(&steps,&failed);
    lv_obj_set_hidden(s_pill_unit,!available);
    if(available)snprintf(value,sizeof(value),"%lu%s",(unsigned long)steps,failed?"\n保存失败":"");
    else snprintf(value,sizeof(value),"计步暂不可用");
    text_if_changed(s_pill_text,value);
    lv_obj_update_layout(s_face);
    /* Long battery status (NO BATTERY/charging/full) gets its own row. */
    bool long_row=lv_obj_get_width(s_face_date)+24+lv_obj_get_width(s_face_power)>338;
    lv_obj_set_flex_flow(s_face_row,long_row?LV_FLEX_FLOW_COLUMN:LV_FLEX_FLOW_ROW);
    lv_obj_update_layout(s_face);
    lv_obj_align(s_face_time,LV_ALIGN_TOP_MID,0,55);
    lv_obj_align(s_face_row,LV_ALIGN_TOP_MID,0,147);
    lv_obj_align(s_pill,LV_ALIGN_TOP_MID,0,long_row?210:186);
    lv_obj_align(s_face,LV_ALIGN_CENTER,dx,dy);
}
