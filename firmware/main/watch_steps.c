/* QMI8658 acceleration + Bangle.js software step counter.
 * SensorLib register conventions retained; only this worker owns the IMU. */
#include "watch_step_algorithm.h"
#include "watch_steps.h"
#include "watch_power.h"
#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include "esp_system.h"
#include <stdio.h>
#include <limits.h>

static const char *TAG = "watch_steps";
static i2c_master_dev_handle_t s_dev;
static SemaphoreHandle_t s_mutex;
static watch_power_client_t s_power;
static uint32_t s_total, s_day, s_requested_day;
static bool s_available, s_saved, s_save_failed;
static bool s_config_started;
static bool s_reinit_requested, s_capture_requested;
static uint32_t s_samples, s_gaps, s_read_errors;
static bool s_diag_requested;
static uint8_t s_original[5];
static const uint8_t s_control_regs[]={2,3,6,8,9};
static esp_err_t read_regs(uint8_t reg, uint8_t *out, size_t size)
{ return i2c_master_transmit_receive(s_dev, &reg, 1, out, size, 100); }
static esp_err_t write_reg(uint8_t reg, uint8_t value)
{ uint8_t data[] = {reg,value}; return i2c_master_transmit(s_dev,data,2,100); }
#define TRY(call) do { esp_err_t e=(call); if(e!=ESP_OK)return e; } while(0)
static esp_err_t configure(void)
{
    uint8_t id, ctrl1, ctrl7, ctrl8, ctrl2, filter;
    TRY(read_regs(0,&id,1));
    if(id!=5)return ESP_ERR_NOT_SUPPORTED;
    TRY(read_regs(2,&ctrl1,1)); TRY(read_regs(8,&ctrl7,1));
    TRY(read_regs(9,&ctrl8,1)); TRY(read_regs(3,&ctrl2,1));
    TRY(read_regs(6,&filter,1));
    /* Only adopt idle IMU or the project's known hardware/official/software
     * configurations. Never steal a gyro or an unknown active motion client. */
    bool known=(ctrl7==1 && ctrl2==7 && (filter&7)==1 &&
                (ctrl8==0x90 || ctrl8==0xd0)) ||
               (ctrl7==0x21 && ctrl2==7 && (filter&7)==5 && ctrl8==0x80);
    if(((ctrl7&3)||(ctrl8&0x1f)) && !known)return ESP_ERR_INVALID_STATE;
    for(unsigned i=0;i<5;i++)TRY(read_regs(s_control_regs[i],&s_original[i],1));
    s_config_started=true;
    TRY(write_reg(8,0));
    TRY(write_reg(9,0x80)); /* disable hardware step/motion engines */
    TRY(write_reg(2,(ctrl1|0x40)&~0x22)); /* auto increment, little endian, awake */
    TRY(write_reg(3,7)); /* ±2g, 62.5Hz; gyro off */
    TRY(write_reg(6,(filter&~7)|5)); /* LPF mode2: 5.39% ODR (~3.37Hz) */
    TRY(write_reg(8,0x21)); /* accelerometer only, no data-ready IRQ */
    const uint8_t expected[]={ (ctrl1|0x40)&~0x22,7,(filter&~7)|5,0x21,0x80 };
    for(unsigned i=0;i<5;i++) {
        uint8_t value;TRY(read_regs(s_control_regs[i],&value,1));
        if(value!=expected[i])return ESP_ERR_INVALID_STATE;
    }
    watch_step_algorithm_reset();
    return ESP_OK;
}
#undef TRY
/* One native NVS u64 entry per date: upper 32 bits date, lower 32 steps.
 * 366 chronological slots use about 12 KB including NVS entry overhead. */
#define HISTORY_DAYS 366
static uint32_t s_history_day[HISTORY_DAYS];
static esp_err_t persist(uint32_t day,uint32_t total)
{
    nvs_handle_t nvs;
    esp_err_t err=nvs_open("pet_steps",NVS_READWRITE,&nvs);
    if(err!=ESP_OK)return err;
    unsigned slot=0;
    if(day) {
        for(unsigned i=0;i<HISTORY_DAYS;i++) {
            if(s_history_day[i]==day){slot=i;break;}
            if(s_history_day[i]<s_history_day[slot])slot=i;
        }
        char key[12];snprintf(key,sizeof(key),"h%03u",slot);
        err=nvs_set_u64(nvs,key,((uint64_t)day<<32)|total);
    }
    uint32_t record[]={1,day,total};
    if(err==ESP_OK)err=nvs_set_blob(nvs,"daily",record,sizeof(record));
    if(err==ESP_OK)err=nvs_commit(nvs);
    if(err==ESP_OK && day)s_history_day[slot]=day;
    nvs_close(nvs);
    return err;
}
/* All persistence paths share s_mutex, including orderly esp_restart().
 * No sensor access or extra network work is performed in the handler. */
static void save_before_restart(void)
{
    if(!s_mutex)return;
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    if(!s_saved) {
        esp_err_t err=persist(s_day,s_total);
        s_saved=err==ESP_OK;s_save_failed=err!=ESP_OK;
    }
    xSemaphoreGive(s_mutex);
}
esp_err_t watch_steps_get_day(uint32_t day,uint32_t *steps)
{
    if(!s_mutex||!day||!steps)return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    esp_err_t err=ESP_ERR_NOT_FOUND;
    if(day==s_day){*steps=s_total;err=ESP_OK;}
    else for(unsigned i=0;i<HISTORY_DAYS;i++)if(s_history_day[i]==day) {
        nvs_handle_t nvs;
        err=nvs_open("pet_steps",NVS_READONLY,&nvs);
        if(err==ESP_OK) {
            char key[12];uint64_t value;
            snprintf(key,sizeof(key),"h%03u",i);
            err=nvs_get_u64(nvs,key,&value);
            if(err==ESP_OK) {
                if((uint32_t)(value>>32)!=day)err=ESP_ERR_INVALID_STATE;
                else *steps=(uint32_t)value;
            }
            nvs_close(nvs);
        }
        break;
    }
    xSemaphoreGive(s_mutex);
    return err;
}
static void worker(void *arg)
{
    (void)arg;
    esp_err_t err=watch_power_client_acquire(s_power);
    bool leased=err==ESP_OK;
    if(leased)err=configure();
    if(err!=ESP_OK) {
        /* Best effort stop after partial configuration; do not keep a lock. */
        if(s_config_started) {
            write_reg(8,0);
            for(unsigned i=0;i<5;i++)write_reg(s_control_regs[i],s_original[i]);
        }
        ESP_LOGW(TAG,"Pedometer unavailable: %s",esp_err_to_name(err));
        if(leased)watch_power_client_release(s_power);
        watch_power_client_delete(s_power);s_power=NULL;
        i2c_master_bus_rm_device(s_dev);s_dev=NULL;
        vTaskDelete(NULL);return;
    }
    watch_power_client_release(s_power);
    ESP_LOGI(TAG,"Software steps: Bangle.js 12.5Hz input; QMI ±2g/62.5Hz LPF2; gyro/embedded steps off");
    int64_t next_sample=esp_timer_get_time();
    int64_t capture_until=0;
    bool dirty=false;
    int64_t next_save=esp_timer_get_time()+60000000;
    int64_t retry_day_at=0;
    for(;;) {
        err=watch_power_client_acquire(s_power);
        uint8_t bytes[11];
        if(err==ESP_OK) {
            xSemaphoreTake(s_mutex,portMAX_DELAY);
            bool reinit=s_reinit_requested;s_reinit_requested=false;
            bool capture=s_capture_requested;s_capture_requested=false;
            bool diagnostic=s_diag_requested;s_diag_requested=false;
            xSemaphoreGive(s_mutex);
            if(capture) { capture_until=esp_timer_get_time()+60000000;watch_step_algorithm_reset(); }
            if(reinit) {
                err=configure();
                if(err!=ESP_OK) {
                    /* Leave the device stopped, keep daily counts recoverable. */
                    write_reg(8,0);
                    xSemaphoreTake(s_mutex,portMAX_DELAY);s_available=false;xSemaphoreGive(s_mutex);
                    watch_power_client_release(s_power);
                    ESP_LOGE(TAG,"Software step reinit failed: %s; worker stopped",esp_err_to_name(err));
                    watch_power_client_delete(s_power);s_power=NULL;
                    i2c_master_bus_rm_device(s_dev);s_dev=NULL;
                    vTaskDelete(NULL);return;
                }
            }
            err=read_regs(0x30,bytes,sizeof(bytes)); /* timestamp, temp, XYZ */
            if(err==ESP_OK) {
                uint32_t timestamp=bytes[0]|((uint32_t)bytes[1]<<8)|((uint32_t)bytes[2]<<16);
                int16_t ax=(int16_t)(bytes[5]|(bytes[6]<<8));
                int16_t ay=(int16_t)(bytes[7]|(bytes[8]<<8));
                int16_t az=(int16_t)(bytes[9]|(bytes[10]<<8));
                bool discontinuity;
                int64_t now=esp_timer_get_time();
                uint32_t delta=watch_step_algorithm_sample(ax,ay,az,timestamp,now,&discontinuity);
                s_samples++;if(discontinuity)s_gaps++;
                if(diagnostic)ESP_LOGI(TAG,"STEP_STATUS software=1 total=%lu samples=%lu gaps=%lu errors=%lu ax=%d ay=%d az=%d ts=%lu",
                    (unsigned long)s_total,(unsigned long)s_samples,(unsigned long)s_gaps,
                    (unsigned long)s_read_errors,ax,ay,az,(unsigned long)timestamp);
                if(now<capture_until)ESP_LOGI(TAG,"STEP_SAMPLE,%lld,%lu,%d,%d,%d,%lu,%u",
                    (long long)now,(unsigned long)timestamp,ax,ay,az,(unsigned long)delta,discontinuity);
                xSemaphoreTake(s_mutex,portMAX_DELAY);
                if(s_requested_day && s_requested_day!=s_day && esp_timer_get_time()>=retry_day_at) {
                    /* A forward date change resets; initial valid date labels
                     * pending steps. Backwards clock correction never resets. */
                    if(s_day) {
                        /* Save the closing date before discarding its RAM value.
                         * On failure retain it and retry; never silently lose it. */
                        /* Attribute the interval ending at this boundary to the
                         * closing date; retain every step even if saving fails. */
                        if(delta)s_total=UINT32_MAX-s_total<delta?UINT32_MAX:s_total+delta;
                        delta=0;
                        esp_err_t save=persist(s_day,s_total);
                        if(save!=ESP_OK) {
                            s_save_failed=true;s_saved=false;dirty=true;
                            retry_day_at=esp_timer_get_time()+60000000;
                            next_save=retry_day_at;
                            s_available=true;
                            xSemaphoreGive(s_mutex);
                            watch_power_client_release(s_power);
                            watch_step_algorithm_reset();
                            vTaskDelay(pdMS_TO_TICKS(80));
                            next_sample=esp_timer_get_time();
                            continue;
                        }
                        s_total=0;delta=0;
                    }
                    s_day=s_requested_day;dirty=true;next_save=0;
                }
                if(delta){s_total=UINT32_MAX-s_total<delta?UINT32_MAX:s_total+delta;dirty=true;}
                if(dirty)s_saved=false;
                s_available=!discontinuity;
                xSemaphoreGive(s_mutex);
            } else {
                s_read_errors++;watch_step_algorithm_reset();
                xSemaphoreTake(s_mutex,portMAX_DELAY);s_available=false;xSemaphoreGive(s_mutex);
            }
            if(dirty && esp_timer_get_time()>=next_save) {
                xSemaphoreTake(s_mutex,portMAX_DELAY);
                uint32_t day=s_day,total=s_total;
                esp_err_t save=persist(day,total);
                s_saved=save==ESP_OK;s_save_failed=save!=ESP_OK;xSemaphoreGive(s_mutex);
                if(save==ESP_OK)dirty=false;
                else ESP_LOGW(TAG,"Step checkpoint failed: %s",esp_err_to_name(save));
                next_save=esp_timer_get_time()+60000000;
            }
            watch_power_client_release(s_power);
        }
        if(err!=ESP_OK) {
            watch_step_algorithm_reset();
            xSemaphoreTake(s_mutex,portMAX_DELAY);s_available=false;xSemaphoreGive(s_mutex);
        }
        /* Deadline-based schedule: no drift and no burst of fake catch-up samples. */
        next_sample+=80000;
        int64_t now=esp_timer_get_time();
        if(next_sample<=now)next_sample=now+80000;
        uint32_t wait_ms=(uint32_t)((next_sample-now+999)/1000);
        vTaskDelay(pdMS_TO_TICKS(wait_ms)>0?pdMS_TO_TICKS(wait_ms):1);
    }
}
esp_err_t watch_steps_init(void)
{
    s_mutex=xSemaphoreCreateMutex();if(!s_mutex)return ESP_ERR_NO_MEM;
    nvs_handle_t nvs;
    if(nvs_open("pet_steps",NVS_READONLY,&nvs)==ESP_OK) {
        uint32_t record[3];size_t size=sizeof(record);
        if(nvs_get_blob(nvs,"daily",record,&size)==ESP_OK && size==sizeof(record) && record[0]==1) {
            s_day=record[1];s_total=record[2];s_saved=true;
        }
        for(unsigned i=0;i<HISTORY_DAYS;i++) {
            char key[12];uint64_t value;
            snprintf(key,sizeof(key),"h%03u",i);
            if(nvs_get_u64(nvs,key,&value)==ESP_OK)s_history_day[i]=(uint32_t)(value>>32);
        }
        nvs_close(nvs);
    }
    esp_err_t err=esp_register_shutdown_handler(save_before_restart);
    if(err!=ESP_OK)return err;
    err=watch_power_client_create("steps",WATCH_POWER_AWAKE,&s_power);
    if(err!=ESP_OK)return err;
    i2c_master_bus_handle_t bus=bsp_i2c_get_handle();
    if(!bus)err=ESP_ERR_INVALID_STATE;
    else {
        unsigned address=i2c_master_probe(bus,0x6b,100)==ESP_OK?0x6b:0x6a;
        i2c_device_config_t config={.dev_addr_length=I2C_ADDR_BIT_LEN_7,.device_address=address,.scl_speed_hz=100000};
        err=i2c_master_bus_add_device(bus,&config,&s_dev);
    }
    if(err==ESP_OK && xTaskCreate(worker,"watch_steps",4096,NULL,1,NULL)!=pdPASS)err=ESP_ERR_NO_MEM;
    if(err!=ESP_OK){if(s_dev)i2c_master_bus_rm_device(s_dev);watch_power_client_delete(s_power);}
    return err;
}
bool watch_steps_get(uint32_t *steps,bool *save_failed)
{
    if(!s_mutex)return false;
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    *steps=s_total;*save_failed=s_save_failed;bool available=s_available;
    xSemaphoreGive(s_mutex);return available;
}
void watch_steps_set_date(const struct tm *t,bool valid)
{
    if(!s_mutex||!valid||!t||t->tm_year<124||t->tm_mon<0||t->tm_mon>11||t->tm_mday<1||t->tm_mday>31)return;
    uint32_t day=(t->tm_year+1900)*10000+(t->tm_mon+1)*100+t->tm_mday;
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    if(day>s_day)s_requested_day=day;
    else s_requested_day=0;
    xSemaphoreGive(s_mutex);
}

void watch_steps_request_diagnostics(void)
{
    if(!s_mutex)return;
    xSemaphoreTake(s_mutex,portMAX_DELAY);s_diag_requested=true;xSemaphoreGive(s_mutex);
}

void watch_steps_request_reinitialize(void)
{
    if(!s_mutex)return;
    xSemaphoreTake(s_mutex,portMAX_DELAY);s_reinit_requested=true;xSemaphoreGive(s_mutex);
}
void watch_steps_request_capture(void)
{
    if(!s_mutex)return;
    xSemaphoreTake(s_mutex,portMAX_DELAY);s_capture_requested=true;xSemaphoreGive(s_mutex);
}
void watch_steps_request_format_test(unsigned format)
{
    (void)format;
    ESP_LOGW(TAG,"Hardware parameter experiments disabled in software step build");
}
