#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"
/* Official 01_AXP2101/XPowersAXP2101.tpp register semantics.
 * Read-only: no charging, rail or interrupt configuration changes. */
typedef struct { bool status_ok,battery,usb,charging,full,percent_ok,voltage_ok; uint8_t percent; uint16_t mv; } watch_pmu_sample_t;
static inline watch_pmu_sample_t watch_pmu_read(i2c_master_dev_handle_t dev,bool voltage) {
 watch_pmu_sample_t s={.percent=255};uint8_t reg=0,b[2];
 if(!dev||i2c_master_transmit_receive(dev,&reg,1,b,2,100)!=ESP_OK)return s;
 s.status_ok=true;s.battery=(b[0]&8)!=0;s.usb=(b[0]&32)&&!(b[1]&8);
 s.charging=(b[1]>>5)==1;s.full=(b[1]&7)==4;
 if(!s.battery)return s;
 reg=0xa4;s.percent_ok=i2c_master_transmit_receive(dev,&reg,1,&s.percent,1,100)==ESP_OK&&s.percent<=100;
 if(voltage){reg=0x34;if(i2c_master_transmit_receive(dev,&reg,1,b,2,100)==ESP_OK){s.mv=((b[0]&31)<<8)|b[1];s.voltage_ok=s.mv>=2000&&s.mv<=5000;}}
 return s;
}
