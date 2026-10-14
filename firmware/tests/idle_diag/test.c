#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include "driver/i2c_master.h"
static unsigned probes,adds,removes,reads,dumps;static bool fail_read,fail_add;
const char *esp_err_to_name(int err){return err?"error":"ok";}
i2c_master_bus_handle_t bsp_i2c_get_handle(void){return (void*)1;}
esp_err_t i2c_master_probe(i2c_master_bus_handle_t bus,unsigned a,int timeout){assert(bus==(void*)1&&timeout==100);probes++;return a==0x6a?ESP_FAIL:ESP_OK;}
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus,const i2c_device_config_t*c,i2c_master_dev_handle_t*d){(void)bus;assert(c->scl_speed_hz==100000);if(fail_add)return ESP_FAIL;adds++;*d=(void*)2;return ESP_OK;}
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t d,const uint8_t*tx,size_t n,uint8_t*rx,size_t k,int t){assert(d==(void*)2&&n==1&&k==1&&t==100);(void)tx;reads++;*rx=0;return fail_read?ESP_FAIL:ESP_OK;}
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t d){assert(d==(void*)2);removes++;return ESP_OK;}
int gpio_dump_io_configuration(FILE*f,uint64_t mask){(void)f;assert(mask==(1ULL<<46));dumps++;return ESP_OK;}
int gpio_get_level(int pin){assert(pin==46);return 0;}
#include "../../main/watch_idle_diag.c"
int main(void){
 watch_idle_diag_snapshot(0);assert(probes==4&&adds==3&&reads==24&&removes==3&&dumps==1);
 watch_idle_diag_snapshot(0);watch_idle_diag_snapshot(2);assert(probes==4);
 fail_read=true;watch_idle_diag_snapshot(1);assert(probes==8&&adds==6&&removes==6&&reads==48);
 s_seen=0;fail_add=true;watch_idle_diag_snapshot(0);assert(adds==6&&removes==6);
 puts("PASS: bounded snapshots, one-byte register pointers only, absent device and failed read/add cleanup");return 0;
}
