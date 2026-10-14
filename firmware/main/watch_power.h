#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#ifndef WATCH_POWER_ENABLED
#define WATCH_POWER_ENABLED 1
#endif
#ifndef WATCH_OPT_DFS
#define WATCH_OPT_DFS 1
#endif
#ifndef WATCH_OPT_TICK
#define WATCH_OPT_TICK 1
#endif
#ifndef WATCH_OPT_SCENE
#define WATCH_OPT_SCENE 1
#endif
#ifndef WATCH_OPT_SLEEP
#define WATCH_OPT_SLEEP 1
#endif

/* Called under the display lock. Bright UI retains a NO_LIGHT_SLEEP lock. */
esp_err_t watch_power_init(void);
void watch_power_set_blanked(bool blanked);
bool watch_power_network_may_stop(void);
esp_err_t watch_power_try_stop_network(void);
bool watch_power_sleep_wifi_off(void);
/* Stored in NVS, default false (retain connection). UI binding is deferred. */
esp_err_t watch_power_set_sleep_wifi_off(bool enabled);

/* Application leases are task-context only, never from an ISR. Acquire for
 * actual work, release on completion/cancel/error; no app owns global PM. */
typedef struct watch_power_client *watch_power_client_t;
enum { WATCH_POWER_CPU = 1, WATCH_POWER_AWAKE = 2, WATCH_POWER_NETWORK = 4 };
esp_err_t watch_power_client_create(const char *name, unsigned needs, watch_power_client_t *out);
esp_err_t watch_power_client_acquire(watch_power_client_t client);
esp_err_t watch_power_client_release(watch_power_client_t client);
esp_err_t watch_power_client_delete(watch_power_client_t client);
void watch_power_dump_clients(void);
void watch_power_client_counts(uint8_t *cpu,uint8_t *awake,uint8_t *network);

esp_err_t watch_power_try_stop_offline_network(void);
void watch_power_sleep_stats(uint64_t *elapsed_us,uint64_t *slept_us);
