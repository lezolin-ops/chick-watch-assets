#pragma once
void vTaskDelay(unsigned);
void vTaskDelete(void*);
int xTaskCreate(void (*fn)(void*),const char*,unsigned,void*,unsigned,void*);
