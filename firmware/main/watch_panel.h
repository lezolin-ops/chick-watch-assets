#pragma once
#include <stdbool.h>
#include "esp_err.h"
/* All calls run under the display lock; no independent timer/task. */
esp_err_t watch_panel_init(void);
esp_err_t watch_panel_sleep(void);
esp_err_t watch_panel_wake(void);
bool watch_panel_is_asleep(void);
