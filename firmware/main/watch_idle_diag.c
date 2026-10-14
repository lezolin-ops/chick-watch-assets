#include "watch_idle_diag.h"
#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <stdint.h>
#include <stdio.h>
#ifndef WATCH_IDLE_DIAGNOSTICS
#define WATCH_IDLE_DIAGNOSTICS 1
#endif
static unsigned s_seen;
static void read_device(i2c_master_bus_handle_t bus,const char *phase,const char *chip,unsigned address,
                        const uint8_t *registers,unsigned count)
{
    esp_err_t err=i2c_master_probe(bus,address,100);
    if(err!=ESP_OK){printf("IDLE_DEVICE,%s,%s,0x%02x,unavailable,%s\n",phase,chip,address,esp_err_to_name(err));return;}
    i2c_master_dev_handle_t dev;
    i2c_device_config_t config={.dev_addr_length=I2C_ADDR_BIT_LEN_7,.device_address=address,.scl_speed_hz=100000};
    err=i2c_master_bus_add_device(bus,&config,&dev);
    if(err!=ESP_OK){printf("IDLE_DEVICE,%s,%s,0x%02x,add_failed,%s\n",phase,chip,address,esp_err_to_name(err));return;}
    for(unsigned i=0;i<count;i++){
        uint8_t value=0,reg=registers[i];
        err=i2c_master_transmit_receive(dev,&reg,1,&value,1,100);
        if(err==ESP_OK)printf("IDLE_REG,%s,%s,0x%02x,0x%02x,0x%02x\n",phase,chip,address,reg,value);
        else printf("IDLE_REG_ERROR,%s,%s,0x%02x,0x%02x,%s\n",phase,chip,address,reg,esp_err_to_name(err));
    }
    err=i2c_master_bus_rm_device(dev);
    if(err!=ESP_OK)ESP_LOGW("idle_diag","Device handle cleanup: %s",esp_err_to_name(err));
}
void watch_idle_diag_snapshot(unsigned phase)
{
    if(!WATCH_IDLE_DIAGNOSTICS||phase>1||(s_seen&(1U<<phase)))return;
    s_seen|=1U<<phase;
    const char *name=phase?"first_black":"boot";
    i2c_master_bus_handle_t bus=bsp_i2c_get_handle();
    if(!bus){ESP_LOGW("idle_diag","I2C unavailable");return;}
    printf("IDLE_BEGIN,%s,read_only\n",name);
    /* Only ID and control/power registers; no IRQ/FIFO status reads. */
    static const uint8_t qmi[]={0x00,0x01,0x02,0x03,0x04,0x08};
    static const uint8_t dac[]={0xfd,0xfe,0x00,0x01,0x02,0x0d,0x0e,0x12,0x14};
    static const uint8_t adc[]={0x00,0x01,0x06,0x47,0x48,0x49,0x4a,0x4b,0x4c};
    read_device(bus,name,"QMI8658",0x6b,qmi,sizeof(qmi));
    read_device(bus,name,"QMI8658_ALT",0x6a,qmi,sizeof(qmi));
    read_device(bus,name,"ES8311",0x18,dac,sizeof(dac));
    read_device(bus,name,"ES7210",0x40,adc,sizeof(adc));
    /* Input level is not valid evidence of an output when input is disabled. */
    printf("IDLE_GPIO,%s,GPIO46,configuration_only\n",name);
    gpio_dump_io_configuration(stdout,1ULL<<46);
    printf("IDLE_GPIO_LEVEL,%s,GPIO46,%d,valid_if_input_enabled\n",name,gpio_get_level(GPIO_NUM_46));
    printf("IDLE_END,%s\n",name);fflush(stdout);
}
