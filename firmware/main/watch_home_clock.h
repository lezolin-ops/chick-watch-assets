#pragma once
#include "lvgl.h"
#include <stdbool.h>
#include <time.h>
void watch_home_clock_create(lv_obj_t *screen);
void watch_home_clock_set_enabled(bool enabled);
void watch_home_clock_update(const struct tm *time, bool valid);
/* Shared PMU status text; no hardware I/O in these presentation functions. */
void watch_status_set_power(const char *text);
void watch_standby_face_create(lv_obj_t *screen);
void watch_standby_face_update(const struct tm *time, bool valid);
