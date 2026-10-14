#pragma once
#include <stdio.h>
#include <stdint.h>
int gpio_dump_io_configuration(FILE *,uint64_t);

#define GPIO_NUM_46 46
int gpio_get_level(int pin);
