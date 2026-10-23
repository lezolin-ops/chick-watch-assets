/* Board adapter for the unmodified SensorLib example shipped by Waveshare.
 * SensorQMI8658.hpp and its register protocol are vendored without edits.
 * The pet shell is retained for display/power; the production step worker is
 * excluded from this build. Test counts are RAM-only, never NVS history. */
#include <cmath>
#include "SensorQMI8658.hpp"
extern "C" {
#include "watch_steps.h"
#include "watch_power.h"
}
#include "bsp/esp-bsp.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
static const char *TAG="official_qmi";
static SensorQMI8658 qmi;
static watch_power_client_t power_client;
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static uint32_t counter,irq_count;
static bool available,diagnostic=true,interrupt_flag,irq_probe_requested;
static i2c_master_dev_handle_t diagnostic_device;
static void IRAM_ATTR irq_handler(void *)
{
    portENTER_CRITICAL_ISR(&mux);interrupt_flag=true;irq_count++;portEXIT_CRITICAL_ISR(&mux);
}
static void pedometer_event()
{
    uint32_t raw=qmi.getPedometerCounter();
    portENTER_CRITICAL(&mux);counter=raw;portEXIT_CRITICAL(&mux);
    ESP_LOGI(TAG,"Detected Pedometer event: %lu",(unsigned long)raw);
}
static void snapshot()
{
    float x=0,y=0,z=0;
    bool read_ok=qmi.getAccelerometer(x,y,z);
    uint32_t raw=qmi.getPedometerCounter(),timestamp=qmi.getTimestamp(),edges;
    uint8_t reg=0x2d,status[3]={};
    esp_err_t se=i2c_master_transmit_receive(diagnostic_device,&reg,1,status,3,100);
    portENTER_CRITICAL(&mux);counter=raw;edges=irq_count;portEXIT_CRITICAL(&mux);
    ESP_LOGI(TAG,"OFFICIAL_STATUS raw=%lu timestamp=%lu acc_ok=%d ax=%.4f ay=%.4f az=%.4f irq=%lu status_ok=%d status=%02x/%02x/%02x",
        (unsigned long)raw,(unsigned long)timestamp,read_ok,x,y,z,(unsigned long)edges,se==ESP_OK,status[0],status[1],status[2]);
}
/* Controlled route test: CTRL9 done can drive INT1 when CTRL8.bit7=0.
 * Never clear a nonzero session count. Always restore and read back CTRL8. */
static void irq_command_probe()
{
    uint32_t raw=qmi.getPedometerCounter();
    if(raw) {ESP_LOGW(TAG,"IRQ_PROBE skipped: nonzero raw=%lu",(unsigned long)raw);return;}
    uint8_t reg=9,original=0;
    esp_err_t err=i2c_master_transmit_receive(diagnostic_device,&reg,1,&original,1,100);
    if(err!=ESP_OK) {ESP_LOGW(TAG,"IRQ_PROBE cannot read CTRL8");return;}
    uint32_t before,after;
    portENTER_CRITICAL(&mux);before=irq_count;portEXIT_CRITICAL(&mux);
    uint8_t command_mode[]={9,(uint8_t)(original&~0x80)};
    err=i2c_master_transmit(diagnostic_device,command_mode,2,100);
    int result=-1;
    if(err==ESP_OK)result=qmi.clearPedometerCounter(); // raw verified zero; original CTRL9 protocol.
    vTaskDelay(pdMS_TO_TICKS(20));
    portENTER_CRITICAL(&mux);after=irq_count;portEXIT_CRITICAL(&mux);
    uint8_t restore[]={9,original};
    esp_err_t restored=i2c_master_transmit(diagnostic_device,restore,2,100);
    uint8_t actual=0;
    esp_err_t readback=i2c_master_transmit_receive(diagnostic_device,&reg,1,&actual,1,100);
    bool safe=restored==ESP_OK&&readback==ESP_OK&&actual==original;
    ESP_LOGI(TAG,"IRQ_PROBE command=%d write_ok=%d edges=%lu pin=%d restored=%d ctrl8=%02x",
        result,err==ESP_OK,(unsigned long)(after-before),gpio_get_level(GPIO_NUM_21),safe,actual);
    if(!safe) {
        // Stop issuing sensor operations until restart; never hide restore failure.
        portENTER_CRITICAL(&mux);available=false;portEXIT_CRITICAL(&mux);
        ESP_LOGE(TAG,"IRQ_PROBE restore failed; restart required");
        watch_power_client_release(power_client);vTaskDelete(nullptr);
    }
}
static void worker(void *)
{
    esp_err_t err=watch_power_client_acquire(power_client);
    bool leased=err==ESP_OK,ok=false;
    if(leased) {
        i2c_master_bus_handle_t bus=bsp_i2c_get_handle();
        uint8_t address=i2c_master_probe(bus,0x6b,100)==ESP_OK?0x6b:0x6a;
        qmi.setPins(21);
        ok=qmi.begin(bus,address); // Original initImpl: soft reset, handshake setup, COPY_USID.
        if(ok) {
            ESP_LOGI(TAG,"OFFICIAL_BEGIN revision=%02x firmware=%06lx address=%02x",qmi.getChipID(),(unsigned long)qmi.getChipFirmwareVersion(),address);
            // Same order and parameter values as QMI8658_PedometerExample.ino.
            qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_2G,SensorQMI8658::ACC_ODR_62_5Hz);
            qmi.enableAccelerometer();
            int config_result=qmi.configPedometer(50,200,100,200,20,10,0,4);
            ok=qmi.enablePedometer(SensorQMI8658::INTERRUPT_PIN_1);
            qmi.setPedometerEventCallBack(pedometer_event);
            ESP_LOGI(TAG,"OFFICIAL_CONFIG result=%d enable=%d; unmodified SensorLib; RAM counts only",config_result,ok);
            // Original library's config result does not propagate all command errors.
            i2c_device_config_t dev={};dev.dev_addr_length=I2C_ADDR_BIT_LEN_7;dev.device_address=address;dev.scl_speed_hz=100000;
            err=i2c_master_bus_add_device(bus,&dev,&diagnostic_device);
            ok=ok&&err==ESP_OK;
            if(ok) {
                uint8_t ctrl[8]={},reg=2;
                err=i2c_master_transmit_receive(diagnostic_device,&reg,1,ctrl,sizeof(ctrl),100);
                ESP_LOGI(TAG,"OFFICIAL_CONTROLS read_ok=%d ctrl1=%02x ctrl2=%02x ctrl5=%02x ctrl7=%02x ctrl8=%02x",err==ESP_OK,ctrl[0],ctrl[1],ctrl[4],ctrl[6],ctrl[7]);
                ok=err==ESP_OK;
            }
            if(ok) {
                /* SensorLib setPins() self-assigns its argument in this
                 * bundled revision. Arduino attachInterrupt enables input;
                 * ESP-IDF gpio_set_intr_type alone does not. */
                gpio_config_t input={};input.pin_bit_mask=1ULL<<GPIO_NUM_21;
                input.mode=GPIO_MODE_INPUT;input.intr_type=GPIO_INTR_ANYEDGE;
                input.pull_up_en=GPIO_PULLUP_DISABLE;input.pull_down_en=GPIO_PULLDOWN_DISABLE;
                err=gpio_config(&input);
                if(err==ESP_OK)err=gpio_install_isr_service(0);
                if(err==ESP_ERR_INVALID_STATE)err=ESP_OK; // BSP may own service.
                if(err==ESP_OK)err=gpio_set_intr_type(GPIO_NUM_21,GPIO_INTR_ANYEDGE);
                if(err==ESP_OK)err=gpio_isr_handler_add(GPIO_NUM_21,irq_handler,nullptr);
                ok=err==ESP_OK;
            }
        }
        watch_power_client_release(power_client);
    }
    portENTER_CRITICAL(&mux);available=ok;portEXIT_CRITICAL(&mux);
    if(!ok) {
        ESP_LOGE(TAG,"OFFICIAL_INIT failed; last adapter error=%s",esp_err_to_name(err));
        watch_power_client_delete(power_client);vTaskDelete(nullptr);return;
    }
    ESP_LOGI(TAG,"OFFICIAL_READY GPIO21 CHANGE; original 500ms loop; extra raw diagnostics every 2s for 10min then on request");
    int64_t until=esp_timer_get_time()+600000000,next=0;
    for(;;) {
        err=watch_power_client_acquire(power_client);
        if(err==ESP_OK) {
            portENTER_CRITICAL(&mux);
            bool irq=interrupt_flag;interrupt_flag=false;bool requested=diagnostic;diagnostic=false;
            bool probe=irq_probe_requested;irq_probe_requested=false;
            portEXIT_CRITICAL(&mux);
            if(irq)qmi.update();
            if(probe)irq_command_probe();
            int64_t now=esp_timer_get_time();
            if(requested||(now<until&&now>=next)) {snapshot();next=now+2000000;}
            watch_power_client_release(power_client);
        }
        vTaskDelay(pdMS_TO_TICKS(500)); // Original example loop interval.
    }
}
extern "C" esp_err_t watch_steps_init(void)
{
    esp_err_t err=watch_power_client_create("official_steps",WATCH_POWER_AWAKE,&power_client);
    if(err!=ESP_OK)return err;
    if(xTaskCreate(worker,"official_steps",6144,nullptr,1,nullptr)!=pdPASS) {
        watch_power_client_delete(power_client);return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
extern "C" bool watch_steps_get(uint32_t *steps,bool *save_failed)
{
    portENTER_CRITICAL(&mux);*steps=counter;*save_failed=false;bool ok=available;portEXIT_CRITICAL(&mux);return ok;
}
extern "C" esp_err_t watch_steps_get_day(uint32_t,uint32_t *) {return ESP_ERR_NOT_SUPPORTED;}
extern "C" void watch_steps_set_date(const struct tm *,bool) {}
extern "C" void watch_steps_request_diagnostics(void) {portENTER_CRITICAL(&mux);diagnostic=true;portEXIT_CRITICAL(&mux);}
extern "C" void watch_steps_request_reinitialize(void) {ESP_LOGW(TAG,"Official adapter: restart device to rerun original setup");}
extern "C" void watch_steps_request_format_test(unsigned) {ESP_LOGW(TAG,"Official adapter: format experiments disabled");}

extern "C" void watch_steps_request_irq_probe(void) {portENTER_CRITICAL(&mux);irq_probe_requested=true;portEXIT_CRITICAL(&mux);}
