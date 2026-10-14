#pragma once
#include <stdbool.h>
#include <stdint.h>
#define WATCH_WIFI_OFFLINE_US (15LL*60*1000000)
static inline bool watch_wifi_offline_due(bool enabled,bool option,bool has_ip,int64_t since,int64_t now) {
 return enabled&&option&&!has_ip&&since>=0&&now>=since&&now-since>=WATCH_WIFI_OFFLINE_US;
}

/* Worker-owned schedule; deadlines use monotonic time, never wall clock. */
typedef struct { unsigned stage; int64_t next_us; } watch_wifi_retry_t;
static inline void watch_wifi_retry_reset(watch_wifi_retry_t *retry) {
 retry->stage=0;retry->next_us=0;
}
static inline unsigned watch_wifi_retry_failed(watch_wifi_retry_t *retry,int64_t now) {
 static const unsigned seconds[]={30,60,120,300};
 unsigned index=retry->stage<4?retry->stage:3;
 unsigned delay=seconds[index];
 if(retry->stage<4)retry->stage++;
 retry->next_us=now+(int64_t)delay*1000000;
 return delay;
}
static inline bool watch_wifi_retry_due(const watch_wifi_retry_t *retry,int64_t now) {
 return now>=retry->next_us;
}
