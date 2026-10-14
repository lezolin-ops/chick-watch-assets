#pragma once
#include "lvgl.h"
#include <stdbool.h>
#include <time.h>
void watch_home_clock_create(lv_obj_t *screen);
void watch_home_clock_set_enabled(bool enabled);
void watch_home_clock_update(const struct tm *time, bool valid);
