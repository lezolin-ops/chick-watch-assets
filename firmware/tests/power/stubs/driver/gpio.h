#pragma once
#include <stdint.h>
#include "esp_err.h"
#define GPIO_NUM_0 0
#define GPIO_NUM_10 10
#define GPIO_MODE_INPUT 0
#define GPIO_PULLUP_DISABLE 0
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_INTR_DISABLE 0
#define GPIO_INTR_HIGH_LEVEL 1
#define GPIO_INTR_LOW_LEVEL 2
typedef struct {uint64_t pin_bit_mask;int mode,pull_up_en,pull_down_en,intr_type;} gpio_config_t;
esp_err_t gpio_config(const gpio_config_t *cfg);
esp_err_t gpio_wakeup_enable(int pin,int level);
esp_err_t gpio_sleep_sel_dis(int pin);
