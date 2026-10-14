#pragma once
#include <pthread.h>
typedef pthread_mutex_t *SemaphoreHandle_t;
#define portMAX_DELAY 0
SemaphoreHandle_t xSemaphoreCreateMutex(void);
int xSemaphoreTake(SemaphoreHandle_t mutex,int ticks);
int xSemaphoreGive(SemaphoreHandle_t mutex);
