#include "watch_settings.h"
#include "watch_wifi.h"
#include "watch_clock.h"
#include "watch_home_clock.h"
#include "lvgl.h"
#include "driver/gpio.h"
#include "bsp/display.h"
#include "nvs.h"
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(watch_font_24);
enum { HOME, NETWORK, PASSWORD, RESULT, BRIGHTNESS, UPDATE };
static lv_obj_t *s_screen, *s_content, *s_title, *s_password, *s_keyboard, *s_value;
static int s_page;
static bool s_active, s_ready, s_waiting;
static unsigned s_brightness = 50, s_revision;
static bool s_home_clock_enabled = true;
static bool s_home_clock_save_failed;
static bool s_offline_save_failed;
static bool s_wifi_toggle_failed;
static wifi_ap_record_t s_selected;
static watch_wifi_status_t s_status;
/* Official LVGL keyboard_2 pattern, enlarged four-row password maps. */
static const char *const lower_map[] = {"q","w","e","r","t","y","u","i","o","p","\n",
    "a","s","d","f","g","h","j","k","l","\n",
    "ABC","z","x","c","v","b","n","m",LV_SYMBOL_BACKSPACE,"\n","1#"," ",LV_SYMBOL_OK,""};
static const char *const upper_map[] = {"Q","W","E","R","T","Y","U","I","O","P","\n",
    "A","S","D","F","G","H","J","K","L","\n",
    "abc","Z","X","C","V","B","N","M",LV_SYMBOL_BACKSPACE,"\n","1#"," ",LV_SYMBOL_OK,""};
static const char *const special_map[] = {"1","2","3","4","5","6","7","8","9","0","\n",
    "!","@","#","$","%","^","&","*","(",")","\n",
    "-","_","=","+","/","?",".",",",":",LV_SYMBOL_BACKSPACE,"\n","abc"," ",LV_SYMBOL_OK,""};
static const char *const extra_map[] = {"[","]","{","}","<",">","\\","|","~","`","\n",
    "\"","'",";","!","@","#","$","%","&","*","\n",
    "-","_","=","+","/","?",".",",",":",LV_SYMBOL_BACKSPACE,"\n","abc"," ",LV_SYMBOL_OK,""};
static lv_buttonmatrix_ctrl_t letter_ctrl[31], symbol_ctrl[33];
static void page(int next);
static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int width)
{
    lv_obj_t *obj = lv_label_create(parent); lv_label_set_text(obj, text);
    lv_obj_set_style_text_color(obj, lv_color_hex(0xF3F5F2), 0);
    lv_obj_set_style_text_font(obj, &watch_font_24, 0);
    lv_obj_set_pos(obj, x, y); lv_obj_set_width(obj, width);
    return obj;
}
static void action(lv_event_t *event);
static lv_obj_t *button(lv_obj_t *parent, const char *text, int x, int y, int w, int h, int code)
{
    lv_obj_t *obj = lv_button_create(parent);
    lv_obj_set_pos(obj, x, y); lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x252B24), 0);
    lv_obj_set_style_shadow_width(obj, 0, 0); lv_obj_set_style_radius(obj, 16, 0);
    if(code == 10 || code == 11 || (s_page == RESULT && code == HOME)) {
        lv_obj_set_style_bg_color(obj, lv_color_hex(0xB5CF83), 0);
        lv_obj_set_style_text_color(obj, lv_color_hex(0x182111), 0);
    }
    lv_obj_t *caption = lv_label_create(obj); lv_label_set_text(caption, text);
    lv_obj_set_width(caption, w - 24); lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(caption);
    lv_obj_add_event_cb(obj, action, LV_EVENT_CLICKED, (void *)(intptr_t)code);
    return obj;
}
static lv_obj_t *home_row(const char *title, const char *detail, int y, int code)
{
    lv_obj_t *row = button(s_content, "", 8, y, 346, 68, code);
    lv_obj_set_style_pad_all(row, 0, 0); lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_scrollable(row, false);
    lv_obj_t *name = label(row, title, 20, 8, 220);
    lv_obj_t *state = label(row, detail, 20, 38, 220);
    lv_obj_set_clickable(name, false); lv_obj_set_clickable(state, false);
    lv_obj_set_style_text_color(state, lv_color_hex(0xA9B0A4), 0);
    lv_obj_t *arrow = label(row, LV_SYMBOL_RIGHT, 304, 22, 24);
    lv_obj_set_clickable(arrow, false);
    return row;
}
static void connect_selected(void)
{
    const char *password = s_password ? lv_textarea_get_text(s_password) : "";
    if(s_selected.authmode != WIFI_AUTH_OPEN && strlen(password) < 8) {
        lv_label_set_text(s_title, "密码至少8位"); return;
    }
    esp_err_t err = watch_wifi_connect(&s_selected, password);
    if(err != ESP_OK) {
        lv_label_set_text(s_title, "网络忙，请稍后重试");
        return;
    }
    s_waiting = true; page(RESULT);
}
static void keyboard_event(lv_event_t *e)
{
    if(lv_event_get_code(e) == LV_EVENT_READY) connect_selected();
    else if(lv_event_get_code(e) == LV_EVENT_CANCEL) page(NETWORK);
}
static void brightness_event(lv_event_t *e)
{
    s_brightness = lv_slider_get_value(lv_event_get_target_obj(e));
    char value[16]; snprintf(value, sizeof(value), "%u%%", s_brightness);
    lv_label_set_text(s_value, value); bsp_display_brightness_set(s_brightness);
    if(lv_event_get_code(e) == LV_EVENT_RELEASED) {
        nvs_handle_t nvs;
        if(nvs_open("pet_display", NVS_READWRITE, &nvs) == ESP_OK) {
            nvs_set_u8(nvs, "brightness", s_brightness); nvs_commit(nvs); nvs_close(nvs);
        }
    }
}
static void action(lv_event_t *event)
{
    int code = (intptr_t)lv_event_get_user_data(event);
    if(code >= 100) {
        if(s_waiting || s_status.state == WATCH_WIFI_SCANNING || s_status.state == WATCH_WIFI_CONNECTING) return;
        unsigned index = code - 100;
        if(index >= s_status.count) return;
        s_selected = s_status.aps[index];
        if(s_selected.authmode == WIFI_AUTH_OPEN) connect_selected(); else page(PASSWORD);
    } else if(code == 10) {
        if(watch_wifi_scan() == ESP_OK) { s_waiting = true; page(NETWORK); }
    } else if(code == 11) connect_selected();
    else if(code == 12 && s_password) lv_textarea_set_password_mode(s_password, !lv_textarea_get_password_mode(s_password));
    else if(code == 13) watch_settings_back();
    else if(code == 14 && s_keyboard) lv_keyboard_set_mode(s_keyboard, LV_KEYBOARD_MODE_USER_1);
    else if(code == 15) {
        s_wifi_toggle_failed=watch_wifi_set_enabled(!s_status.enabled)!=ESP_OK;
        if(!s_wifi_toggle_failed)s_waiting=false;
        page(NETWORK);
    }
    else if(code == 17) {
        s_offline_save_failed=watch_wifi_set_offline_auto_off(!s_status.offline_auto_off)!=ESP_OK;
        page(NETWORK);
    }
    else if(code == 16) {
        bool next = !s_home_clock_enabled;
        nvs_handle_t nvs;
        esp_err_t err = nvs_open("pet_display", NVS_READWRITE, &nvs);
        if(err == ESP_OK) {
            err = nvs_set_u8(nvs, "home_clock", next);
            if(err == ESP_OK) err = nvs_commit(nvs);
            nvs_close(nvs);
        }
        s_home_clock_save_failed = err != ESP_OK;
        if(!s_home_clock_save_failed) {
            s_home_clock_enabled = next;
            watch_home_clock_set_enabled(next);
        }
        page(HOME);
    }
    else if(code == NETWORK) {
        if(!s_ready) { page(RESULT); return; }
        if(watch_wifi_scan() == ESP_OK) s_waiting = true;
        page(NETWORK);
    } else page(code);
}
static void network_switch_row(const char *name,const char *state_text,int y,bool enabled,int code)
{
    lv_obj_t *row=button(s_content,"",8,y,346,68,code);
    lv_obj_set_style_pad_all(row,0,0);lv_obj_set_style_border_width(row,0,0);
    lv_obj_set_scrollable(row,false);
    lv_obj_t *name_label=label(row,name,20,8,220);lv_obj_set_clickable(name_label,false);
    lv_obj_t *state_label=label(row,state_text,20,38,220);lv_obj_set_clickable(state_label,false);
    lv_obj_set_style_text_color(state_label,lv_color_hex(0xA9B0A4),0);
    lv_obj_t *toggle=lv_switch_create(row);
    lv_obj_set_pos(toggle,254,10);lv_obj_set_size(toggle,76,48);
    lv_obj_set_style_bg_color(toggle,lv_color_hex(0xB5CF83),LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(toggle,lv_color_hex(0x182111),LV_PART_KNOB);
    if(enabled)lv_obj_add_state(toggle,LV_STATE_CHECKED);
    lv_obj_add_event_cb(toggle,action,LV_EVENT_VALUE_CHANGED,(void *)(intptr_t)code);
}
static void page(int next)
{
    s_page = next;
    if(s_keyboard) lv_obj_delete(s_keyboard);
    s_password = s_keyboard = NULL;
    lv_obj_clean(s_content); watch_wifi_status(&s_status); s_revision = s_status.revision;
    const char *titles[] = {"设置", "Wi-Fi", "输入密码", "连接 Wi-Fi", "屏幕亮度", "系统更新"};
    lv_label_set_text(s_title, titles[next]);
    if(next == HOME) {
        home_row("网络", s_status.state == WATCH_WIFI_CONNECTED ? "已连接" : s_status.enabled ? "未连接" : "已关闭", 18, NETWORK);
        char text[48]; snprintf(text, sizeof(text), "当前 %u%%", s_brightness);
        home_row("屏幕亮度", text, 96, BRIGHTNESS);
        lv_obj_t *row = button(s_content, "", 8, 174, 346, 68, 16);
        lv_obj_set_style_pad_all(row, 0, 0); lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_scrollable(row, false);
        lv_obj_t *name = label(row, "桌面时钟", 20, 8, 210);
        lv_obj_set_clickable(name, false);
        lv_obj_t *state = label(row, s_home_clock_save_failed ? "未保存，请重试" :
            s_home_clock_enabled ? "已开启" : "已关闭", 20, 38, 210);
        lv_obj_set_clickable(state, false);
        lv_obj_set_style_text_color(state, lv_color_hex(0xA9B0A4), 0);
        lv_obj_t *toggle = lv_switch_create(row);
        lv_obj_set_pos(toggle, 254, 10); lv_obj_set_size(toggle, 76, 48);
        lv_obj_set_style_bg_color(toggle, lv_color_hex(0xB5CF83), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(toggle, lv_color_hex(0x182111), LV_PART_KNOB);
        if(s_home_clock_enabled) lv_obj_add_state(toggle, LV_STATE_CHECKED);
        lv_obj_add_event_cb(toggle, action, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)16);
        home_row("系统更新", "功能待接入", 252, UPDATE);
        lv_obj_t *hint = label(s_content, "按 BOOT 返回", 66, 340, 230);
        lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    } else if(next == NETWORK) {
        network_switch_row("Wi-Fi",s_wifi_toggle_failed ? "请重试" : s_status.enabled ? "已开启" : "已关闭",0,s_status.enabled,15);
        network_switch_row("离线15分钟关闭",s_offline_save_failed ? "未保存，请重试" :
            s_status.offline_auto_off ? "已开启" : "已关闭",76,s_status.offline_auto_off,17);
        lv_obj_t *network_status=label(s_content,!s_ready ? "网络初始化失败，请重启" :
            !s_status.enabled ? (s_status.auto_stopped ? "离线超时，已关闭" : "Wi-Fi 已关闭") :
            s_waiting || s_status.state==WATCH_WIFI_SCANNING ? "正在搜索 Wi-Fi…" :
            s_status.state==WATCH_WIFI_FAILED ? s_status.message : "2.4 GHz Wi-Fi",8,148,346);
        lv_label_set_long_mode(network_status,LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_height(network_status,40);
        lv_obj_t *list = lv_obj_create(s_content);
        lv_obj_set_pos(list, 0, 192); lv_obj_set_size(list, 362, 100);
        lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(list, 0, 0);
        lv_obj_set_style_pad_all(list, 0, 0); lv_obj_set_scroll_dir(list, LV_DIR_VER);
        for(unsigned i = 0; s_status.enabled && i < s_status.count; i++) {
            char text[96]; snprintf(text, sizeof(text), "%s  %s", s_status.aps[i].ssid,
                s_status.aps[i].authmode == WIFI_AUTH_OPEN ? "" : "*");
            lv_obj_t *b = button(list, text, 8, i * 76, 346, 68, 100 + i);
            if(s_status.aps[i].authmode != WIFI_AUTH_OPEN && s_status.aps[i].authmode != WIFI_AUTH_WPA2_PSK &&
               s_status.aps[i].authmode != WIFI_AUTH_WPA_WPA2_PSK && s_status.aps[i].authmode != WIFI_AUTH_WPA3_PSK &&
               s_status.aps[i].authmode != WIFI_AUTH_WPA2_WPA3_PSK) lv_obj_add_state(b, LV_STATE_DISABLED);
        }
        if(s_status.enabled && !s_status.count && !s_waiting && s_status.state != WATCH_WIFI_SCANNING)
            label(list, "点击刷新列表搜索网络", 8, 16, 340);
        lv_obj_t *refresh = button(s_content, "刷新列表", 8, 292, 346, 56, 10);
        if(!s_status.enabled || s_waiting || s_status.state == WATCH_WIFI_SCANNING || s_status.state == WATCH_WIFI_CONNECTING)
            lv_obj_add_state(refresh, LV_STATE_DISABLED);
    } else if(next == PASSWORD) {
        label(s_content, (const char *)s_selected.ssid, 8, 0, 256);
        button(s_content, "#+=", 272, 0, 90, 36, 14);
        s_password = lv_textarea_create(s_content);
        lv_obj_set_pos(s_password, 4, 42); lv_obj_set_size(s_password, 248, 58);
        lv_textarea_set_one_line(s_password, true); lv_textarea_set_password_mode(s_password, true);
        lv_textarea_set_max_length(s_password, 63); lv_textarea_set_placeholder_text(s_password, "Wi-Fi 密码");
        lv_obj_set_style_bg_color(s_password, lv_color_hex(0x252B24), 0);
        lv_obj_set_style_text_color(s_password, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_color(s_password, lv_color_hex(0xFFFFFF), LV_PART_CURSOR);
        lv_obj_set_style_text_color(s_password, lv_color_hex(0xA9B0A4), LV_PART_TEXTAREA_PLACEHOLDER);
        lv_obj_set_style_border_color(s_password, lv_color_hex(0x4C5548), 0);
        button(s_content, LV_SYMBOL_EYE_OPEN, 260, 42, 48, 58, 12);
        button(s_content, LV_SYMBOL_OK, 314, 42, 48, 58, 11);
        s_keyboard = lv_keyboard_create(s_screen);
        lv_obj_set_align(s_keyboard, LV_ALIGN_TOP_LEFT);
        lv_obj_set_pos(s_keyboard, 24, 210); lv_obj_set_size(s_keyboard, 362, 230);
        lv_obj_set_style_text_font(s_keyboard, &lv_font_montserrat_24, 0);
        lv_obj_set_style_bg_color(s_keyboard, lv_color_hex(0x090B0B), 0);
        lv_obj_set_style_bg_color(s_keyboard, lv_color_hex(0x252B24), LV_PART_ITEMS);
        lv_obj_set_style_text_color(s_keyboard, lv_color_hex(0xF3F5F2), LV_PART_ITEMS);
        lv_obj_set_style_pad_row(s_keyboard, 8, 0); lv_obj_set_style_pad_column(s_keyboard, 4, 0);
        lv_keyboard_set_map(s_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER, lower_map, letter_ctrl);
        lv_keyboard_set_map(s_keyboard, LV_KEYBOARD_MODE_TEXT_UPPER, upper_map, letter_ctrl);
        lv_keyboard_set_map(s_keyboard, LV_KEYBOARD_MODE_SPECIAL, special_map, symbol_ctrl);
        lv_keyboard_set_map(s_keyboard, LV_KEYBOARD_MODE_USER_1, extra_map, symbol_ctrl);
        lv_keyboard_set_mode(s_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
        lv_keyboard_set_popovers(s_keyboard, true); lv_keyboard_set_textarea(s_keyboard, s_password);
        lv_obj_add_event_cb(s_keyboard, keyboard_event, LV_EVENT_ALL, NULL);
        lv_obj_t *space = label(s_keyboard, "空格", 0, 0, 72);
        lv_obj_set_style_text_align(space, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(space, LV_ALIGN_BOTTOM_MID, 0, -16);
        lv_obj_set_clickable(space, false);
    } else if(next == RESULT) {
        label(s_content, s_waiting || s_status.state == WATCH_WIFI_CONNECTING ? "正在连接…" :
            !s_ready ? "网络初始化失败，请重启" :
            s_status.state == WATCH_WIFI_CONNECTED ? "已连接" : "连接未成功", 8, 22, 346);
        label(s_content, s_status.ssid, 8, 82, 346);
        label(s_content, s_waiting ? "请稍候" : s_status.message, 8, 134, 346);
        if(s_status.state == WATCH_WIFI_CONNECTED && !s_waiting) label(s_content, s_status.ip, 8, 210, 346);
        button(s_content, s_status.state == WATCH_WIFI_CONNECTED ? "完成" : "返回网络列表", 8, 272, 346, 64,
               s_status.state == WATCH_WIFI_CONNECTED ? HOME : NETWORK);
    } else if(next == BRIGHTNESS) {
        char text[16]; snprintf(text, sizeof(text), "%u%%", s_brightness);
        s_value = label(s_content, text, 8, 30, 346);
        lv_obj_t *slider = lv_slider_create(s_content); lv_obj_set_pos(slider, 20, 120); lv_obj_set_size(slider, 318, 24);
        lv_obj_set_style_bg_color(slider, lv_color_hex(0xB5CF83), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(slider, lv_color_hex(0xB5CF83), LV_PART_KNOB);
        lv_slider_set_range(slider, 10, 100); lv_slider_set_value(slider, s_brightness, LV_ANIM_OFF);
        lv_obj_add_event_cb(slider, brightness_event, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(slider, brightness_event, LV_EVENT_RELEASED, NULL);
        label(s_content, "拖动调节，松手保存\n时钟页保持低亮度", 8, 190, 346);
    } else label(s_content, "更新功能待接入", 8, 40, 346);
}
bool watch_settings_active(void) { return s_active; }
unsigned watch_settings_brightness(void) { return s_brightness; }
void watch_settings_exit(void) { page(HOME); s_active = false; watch_clock_settings_close(); }
void watch_settings_back(void)
{
    if(!s_active) return;
    if(s_page != HOME) { s_waiting = false; page(HOME); return; }
    watch_settings_exit();
}
static void tick(lv_timer_t *timer)
{
    (void)timer;
    static bool pressed; static uint32_t edge;
    bool down = !gpio_get_level(GPIO_NUM_0);
    if(down != pressed && lv_tick_get() - edge > 50) {
        edge = lv_tick_get(); pressed = down;
        if(!down) {
            lv_display_trigger_activity(NULL);
            if(s_active) watch_settings_back();
            else { s_active = true; s_waiting = false; watch_clock_settings_open(s_screen); page(HOME); }
        }
    }
    if(!s_active) return;
    if(lv_display_get_inactive_time(NULL) > 60000) {
        watch_settings_exit(); return;
    }
    watch_wifi_status_t status; watch_wifi_status(&status);
    if(status.revision != s_revision && (s_page == NETWORK || s_page == RESULT)) {
        if(status.state != WATCH_WIFI_CONNECTING && status.state != WATCH_WIFI_SCANNING) s_waiting = false;
        page(s_page);
    }
}
void watch_settings_init(void)
{
    for(unsigned i=0; i<31; i++) letter_ctrl[i] = 1 | LV_BUTTONMATRIX_CTRL_POPOVER;
    letter_ctrl[19] = letter_ctrl[27] = 2;
    letter_ctrl[28] = letter_ctrl[30] = 2; letter_ctrl[29] = 5;
    for(unsigned i=0; i<33; i++) symbol_ctrl[i] = 1 | LV_BUTTONMATRIX_CTRL_POPOVER;
    symbol_ctrl[29] = 2; symbol_ctrl[30] = symbol_ctrl[32] = 2; symbol_ctrl[31] = 5;
    s_ready = watch_wifi_init() == ESP_OK;
    nvs_handle_t nvs; uint8_t brightness;
    if(nvs_open("pet_display", NVS_READONLY, &nvs) == ESP_OK) {
        if(nvs_get_u8(nvs, "brightness", &brightness) == ESP_OK && brightness >= 10 && brightness <= 100)
            s_brightness = brightness;
        uint8_t enabled;
        if(nvs_get_u8(nvs, "home_clock", &enabled) == ESP_OK && enabled <= 1)
            s_home_clock_enabled = enabled;
        nvs_close(nvs);
    }
    watch_home_clock_set_enabled(s_home_clock_enabled);
    gpio_config_t cfg = {.pin_bit_mask = 1ULL << GPIO_NUM_0, .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&cfg);
    s_screen = lv_obj_create(NULL); lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x090B0B), 0);
    lv_obj_set_style_text_color(s_screen, lv_color_hex(0xF3F5F2), 0);
    lv_obj_set_style_text_font(s_screen, &watch_font_24, 0); lv_obj_set_scrollable(s_screen, false);
    button(s_screen, LV_SYMBOL_LEFT, 56, 40, 48, 48, 13);
    s_title = label(s_screen, "设置", 120, 50, 234);
    s_content = lv_obj_create(s_screen); lv_obj_set_pos(s_content, 24, 94); lv_obj_set_size(s_content, 362, 380);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(s_content, 0, 0);
    lv_obj_set_style_pad_all(s_content, 0, 0); lv_obj_set_scrollable(s_content, false);
    lv_timer_create(tick, 50, NULL);
    if(!s_ready) { /* Initialization failure is shown instead of a fake connection state. */
        s_status.state = WATCH_WIFI_FAILED;
    }
}
