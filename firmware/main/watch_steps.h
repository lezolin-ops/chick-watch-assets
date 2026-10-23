#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "esp_err.h"
esp_err_t watch_steps_init(void);
/* UI-thread snapshot; performs no sensor I/O. */
bool watch_steps_get(uint32_t *steps, bool *save_failed);
void watch_steps_set_date(const struct tm *date, bool valid);

/* YYYYMMDD in system Beijing time; last 366 recorded dates, no UI I/O. */
esp_err_t watch_steps_get_day(uint32_t day, uint32_t *steps);

/* Queues one sensor snapshot for the worker; no I/O in the caller. */
void watch_steps_request_diagnostics(void);

/* Explicit diagnostic request, executed by the sensor worker. */
void watch_steps_request_reinitialize(void);

/* Temporary diagnostic format 1..3, restores baseline after 120 seconds. */
void watch_steps_request_format_test(unsigned format);

/* Available only in the official diagnostic adapter build. */
void watch_steps_request_irq_probe(void);

/* Explicit 60-second raw CSV capture; never active by default. */
void watch_steps_request_capture(void);
