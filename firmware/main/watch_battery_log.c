#include "watch_battery_log.h"
#include "watch_pmu.h"
#include "watch_battery_log_format.h"
#include "watch_power.h"
#include "watch_wifi.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "driver/usb_serial_jtag.h"
#include "freertos/task.h"
#include "nvs.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifndef WATCH_BATTERY_LOG_ENABLED
#define WATCH_BATTERY_LOG_ENABLED 1
#endif
static const char *TAG="battery_log",*SPACE="pet_batlog";
static batlog_chunk_t s_chunk;
static uint32_t s_seq;
static int64_t s_next_us;
static unsigned s_dirty;
static bool s_ready,s_exported;
static TaskHandle_t s_export_task;
static uint64_t s_last_elapsed,s_last_slept;
static uint32_t s_last_attempts;
static void key_for(unsigned slot,char key[8]){snprintf(key,8,"c%u",slot);}
static bool read_chunk(nvs_handle_t nvs,unsigned slot,batlog_chunk_t *c) {
 char key[8];key_for(slot,key);size_t size=sizeof(*c);
 return nvs_get_blob(nvs,key,c,&size)==ESP_OK&&batlog_valid(c,size,slot);
}
static esp_err_t save_chunk(void) {
 if(!s_dirty)return ESP_OK;
 s_chunk.crc=batlog_crc(s_chunk.records,s_chunk.count*sizeof(batlog_record_t));
 nvs_handle_t nvs;esp_err_t err=nvs_open(SPACE,NVS_READWRITE,&nvs);
 if(err==ESP_OK){char key[8];key_for((s_chunk.start_seq/BATLOG_RECORDS)%BATLOG_SLOTS,key);
  err=nvs_set_blob(nvs,key,&s_chunk,batlog_size(&s_chunk));
  if(err==ESP_OK)err=nvs_commit(nvs);
  nvs_close(nvs);
 }
 if(err==ESP_OK)s_dirty=0;
 else {ESP_LOGW(TAG,"Save failed: %s; recording stopped, settings retained",esp_err_to_name(err));s_ready=false;}
 return err;
}
static void export_worker(void *arg) {
 (void)arg;batlog_chunk_t *c=malloc(sizeof(*c));
 if(!c){ESP_LOGW(TAG,"Export buffer unavailable");vTaskDelete(NULL);return;}
 for(;;){ulTaskNotifyTake(pdTRUE,portMAX_DELAY);vTaskDelay(pdMS_TO_TICKS(10000));
  if(!usb_serial_jtag_is_connected())continue;
  nvs_handle_t nvs;if(nvs_open(SPACE,NVS_READONLY,&nvs)!=ESP_OK)continue;
  printf("BATLOG_BEGIN,1,interval_s=180,capacity=512\n");
  printf("BATLOG_HEADER,seq,epoch_s,uptime_s,battery_mv,percent,flags,screen,brightness,cpu_clients,awake_clients,network_clients,sleep_path_percent,connect_attempts\n");
  for(unsigned slot=0;slot<BATLOG_SLOTS;slot++)if(read_chunk(nvs,slot,c))for(unsigned i=0;i<c->count;i++){
   const batlog_record_t *r=&c->records[i];
   printf("BATLOG,%lu,%lu,%lu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",(unsigned long)r->seq,
    (unsigned long)r->epoch_s,(unsigned long)r->uptime_s,r->battery_mv,r->percent,r->flags,
    r->screen,r->brightness,r->cpu_clients,r->awake_clients,r->network_clients,
    (r->flags&32768)?r->reserved:255,(r->flags&32768)?r->reserved2:255);
  }
  nvs_close(nvs);printf("BATLOG_END\n");esp_pm_dump_locks(stdout);fflush(stdout);
 }
}
esp_err_t watch_battery_log_init(void) {
 if(!WATCH_BATTERY_LOG_ENABLED)return ESP_OK;
 batlog_chunk_t *c=malloc(sizeof(*c));if(!c)return ESP_ERR_NO_MEM;
 nvs_handle_t nvs;
 if(nvs_open(SPACE,NVS_READONLY,&nvs)==ESP_OK){
  for(unsigned slot=0;slot<BATLOG_SLOTS;slot++)if(read_chunk(nvs,slot,c)){
   uint32_t next=c->start_seq+c->count;if(next>s_seq){s_seq=next;s_chunk=*c;}
  }nvs_close(nvs);
 }free(c);
 if(xTaskCreate(export_worker,"battery_export",6144,NULL,1,&s_export_task)!=pdPASS)return ESP_ERR_NO_MEM;
 s_ready=true;ESP_LOGI(TAG,"Recording every 180s; 512 samples (~25.6h); flush every 2 samples");return ESP_OK;
}
void watch_battery_log_tick(i2c_master_dev_handle_t pmu,unsigned screen,unsigned brightness,bool valid_time) {
 if(!s_ready)return;
 int64_t now=esp_timer_get_time();
 if(now<s_next_us)return;
 s_next_us=now+(int64_t)BATLOG_INTERVAL_S*1000000;
 if(s_seq%BATLOG_RECORDS==0){if(save_chunk()!=ESP_OK)return;memset(&s_chunk,0,sizeof(s_chunk));
  s_chunk.magic=BATLOG_MAGIC;s_chunk.version=BATLOG_VERSION;s_chunk.record_size=24;s_chunk.start_seq=s_seq;
 }
 batlog_record_t r={.seq=s_seq,.epoch_s=(uint32_t)time(NULL),.uptime_s=(uint32_t)(now/1000000),
  .percent=255,.screen=screen,.brightness=brightness};
 if(valid_time)r.flags|=B_TIME_OK;
 watch_pmu_sample_t power=watch_pmu_read(pmu,true);
 if(power.status_ok)r.flags|=B_STATUS_OK;
 if(power.battery)r.flags|=B_BATTERY;
 if(power.usb)r.flags|=B_USB;
 if(power.charging)r.flags|=B_CHARGING;
 if(power.voltage_ok){r.battery_mv=power.mv;r.flags|=B_VOLT_OK;}
 if(power.percent_ok){r.percent=power.percent;r.flags|=B_PERCENT_OK;}
 watch_wifi_status_t wifi;watch_wifi_status(&wifi);if(wifi.enabled)r.flags|=B_WIFI;if(wifi.ip[0])r.flags|=B_IP;
 if(watch_power_sleep_wifi_off())r.flags|=B_WIFI_OFF_POLICY;
 if(WATCH_POWER_ENABLED)r.flags|=B_POWER;
 if(WATCH_OPT_DFS)r.flags|=B_DFS;
 if(WATCH_OPT_TICK)r.flags|=B_TICK;
 if(WATCH_OPT_SCENE)r.flags|=B_SCENE;
 if(WATCH_OPT_SLEEP)r.flags|=B_SLEEP;
 uint64_t elapsed,slept;watch_power_sleep_stats(&elapsed,&slept);
 if(s_last_elapsed&&elapsed>s_last_elapsed){
  uint64_t ratio=100*(slept-s_last_slept)/(elapsed-s_last_elapsed);
  r.reserved=ratio>100?100:ratio;r.flags|=32768;
  uint32_t attempts=wifi.connect_attempts-s_last_attempts;r.reserved2=attempts>254?254:attempts;
 }
 s_last_elapsed=elapsed;s_last_slept=slept;s_last_attempts=wifi.connect_attempts;
 watch_power_client_counts(&r.cpu_clients,&r.awake_clients,&r.network_clients);
 s_chunk.records[s_chunk.count++]=r;s_seq++;s_dirty++;
 if(s_dirty>=2||s_chunk.count==BATLOG_RECORDS||s_seq==1||((r.flags&B_PERCENT_OK)&&r.percent<=5))save_chunk();
 bool host=usb_serial_jtag_is_connected();if(host&&!s_exported){save_chunk();xTaskNotifyGive(s_export_task);}s_exported=host;
}
