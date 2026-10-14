#pragma once
#include "driver/i2c_master.h"
#include <stdbool.h>
#include "esp_err.h"
enum { BATLOG_PET, BATLOG_CLOCK, BATLOG_BLACK, BATLOG_SETTINGS };
esp_err_t watch_battery_log_init(void);
void watch_battery_log_tick(i2c_master_dev_handle_t pmu,unsigned screen,unsigned brightness,bool time_valid);
