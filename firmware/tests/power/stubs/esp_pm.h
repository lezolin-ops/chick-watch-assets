#pragma once
#include "esp_err.h"
typedef struct test_pm_lock {int type, held;} *esp_pm_lock_handle_t;
typedef struct {int max_freq_mhz,min_freq_mhz;bool light_sleep_enable;} esp_pm_config_t;
#define ESP_PM_CPU_FREQ_MAX 1
#define ESP_PM_NO_LIGHT_SLEEP 2
esp_err_t esp_pm_lock_create(int type,int arg,const char *name,esp_pm_lock_handle_t *out);
esp_err_t esp_pm_lock_acquire(esp_pm_lock_handle_t lock);
esp_err_t esp_pm_lock_release(esp_pm_lock_handle_t lock);
esp_err_t esp_pm_lock_delete(esp_pm_lock_handle_t lock);
esp_err_t esp_pm_configure(const esp_pm_config_t *config);
