#pragma once
#include <stdint.h>
#include <stdbool.h>
/* Singleton: owned exclusively by the sensor worker. */
void watch_step_algorithm_reset(void);
/* QMI ±2g signed raw (16384/g), sensor timestamp and host monotonic µs.
 * Samples must arrive at 12.5 Hz. Invalid timing/duplicates never add steps. */
unsigned watch_step_algorithm_sample(int16_t x, int16_t y, int16_t z,
                                    uint32_t timestamp, int64_t now_us,
                                    bool *discontinuity);
