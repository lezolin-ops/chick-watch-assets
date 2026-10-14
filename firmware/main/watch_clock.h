#pragma once
#include "lvgl.h"
void watch_clock_create(lv_obj_t *pet_screen);
void watch_clock_settings_open(lv_obj_t *screen);
void watch_clock_settings_close(void);
void watch_clock_network_time_ready(void);
