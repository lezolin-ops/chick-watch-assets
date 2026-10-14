#pragma once
#include <stdint.h>
#include <string.h>
typedef int nvs_handle_t;
#define NVS_READWRITE 0
#define NVS_READONLY 1
static int test_nvs_fail;
static uint8_t test_nvs_clock=1;
static inline int nvs_open(const char *s,int mode,nvs_handle_t *h){(void)s;(void)mode;*h=1;return test_nvs_fail;}
static inline int nvs_get_u8(nvs_handle_t h,const char *s,uint8_t *b){(void)h;*b=!strcmp(s,"home_clock")?test_nvs_clock:50;return 0;}
static inline int nvs_set_u8(nvs_handle_t h,const char *s,uint8_t b){(void)h;if(!strcmp(s,"home_clock"))test_nvs_clock=b;return test_nvs_fail;}
static inline int nvs_commit(nvs_handle_t h){(void)h;return test_nvs_fail;}
static inline void nvs_close(nvs_handle_t h){(void)h;}
