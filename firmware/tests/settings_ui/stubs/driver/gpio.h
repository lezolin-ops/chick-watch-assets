#pragma once
#include <stdint.h>
#define GPIO_NUM_0 0
#define GPIO_MODE_INPUT 0
#define GPIO_PULLUP_ENABLE 1
#define GPIO_INTR_DISABLE 0
typedef struct {uint64_t pin_bit_mask; int mode,pull_up_en,intr_type;} gpio_config_t;
extern int test_gpio_level;
static inline int gpio_get_level(int pin) {(void)pin; return test_gpio_level;}
static inline int gpio_config(const gpio_config_t *c) {(void)c; return 0;}
