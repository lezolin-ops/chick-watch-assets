#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../../main/watch_steps.c"
static uint8_t regs[256];
static int64_t clock_us;
static bool fail_read,fail_save,walk,corrupt_config;
static int held,cycles,stop_after,write_count,commits,fail_at;
static uint32_t checkpoint[3];
static uint64_t history[HISTORY_DAYS];
static jmp_buf end;
const char *esp_err_to_name(esp_err_t e){(void)e;return "test";}
int64_t esp_timer_get_time(void){return clock_us;}
SemaphoreHandle_t xSemaphoreCreateMutex(void){return (void*)1;}
int xSemaphoreTake(SemaphoreHandle_t m,unsigned t){(void)m;(void)t;return 1;}
int xSemaphoreGive(SemaphoreHandle_t m){(void)m;return 1;}
esp_err_t esp_register_shutdown_handler(void(*fn)(void)){(void)fn;return 0;}
i2c_master_bus_handle_t bsp_i2c_get_handle(void){return (void*)1;}
esp_err_t i2c_master_probe(i2c_master_bus_handle_t b,unsigned a,int t){(void)b;(void)a;(void)t;return 0;}
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t b,const i2c_device_config_t*c,i2c_master_dev_handle_t*d){(void)b;(void)c;*d=(void*)1;return 0;}
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t d){(void)d;return 0;}
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t d,const uint8_t*r,size_t n,uint8_t*out,size_t z,int t){
 (void)d;(void)n;(void)t;if(fail_read||(*r==0x30 && cycles==fail_at))return ESP_FAIL;
 memcpy(out,regs+*r,z);if(corrupt_config&&*r==3)out[0]^=1;return 0;
}
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t d,const uint8_t*b,size_t n,int t){
 (void)d;(void)t;assert(n==2);write_count++;regs[b[0]]=b[1];return 0;
}
esp_err_t watch_power_client_create(const char*n,unsigned needs,watch_power_client_t*out){(void)n;assert(needs==WATCH_POWER_AWAKE);*out=(void*)1;return 0;}
esp_err_t watch_power_client_acquire(watch_power_client_t p){(void)p;assert(!held);held++;return 0;}
esp_err_t watch_power_client_release(watch_power_client_t p){(void)p;assert(held==1);held--;return 0;}
esp_err_t watch_power_client_delete(watch_power_client_t p){(void)p;assert(!held);return 0;}
esp_err_t nvs_open(const char*n,int mode,nvs_handle_t*out){(void)n;(void)mode;*out=1;return 0;}
esp_err_t nvs_get_blob(nvs_handle_t n,const char*k,void*b,size_t*z){(void)n;(void)k;memcpy(b,checkpoint,*z);return 0;}
esp_err_t nvs_set_blob(nvs_handle_t n,const char*k,const void*b,size_t z){(void)n;(void)k;if(fail_save)return ESP_FAIL;memcpy(checkpoint,b,z);return 0;}
esp_err_t nvs_set_u64(nvs_handle_t n,const char*k,uint64_t v){(void)n;if(fail_save)return ESP_FAIL;unsigned i;assert(sscanf(k,"h%u",&i)==1&&i<HISTORY_DAYS);history[i]=v;return 0;}
esp_err_t nvs_get_u64(nvs_handle_t n,const char*k,uint64_t*v){(void)n;unsigned i;assert(sscanf(k,"h%u",&i)==1&&i<HISTORY_DAYS);if(!history[i])return ESP_ERR_NOT_FOUND;*v=history[i];return 0;}
esp_err_t nvs_commit(nvs_handle_t n){(void)n;commits++;return fail_save?ESP_FAIL:0;}
void nvs_close(nvs_handle_t n){(void)n;}
int xTaskCreate(void(*fn)(void*),const char*n,unsigned stack,void*a,unsigned priority,void*out){(void)fn;(void)n;(void)stack;(void)a;(void)priority;(void)out;return pdPASS;}
void vTaskDelete(void*p){(void)p;longjmp(end,1);}
void vTaskDelay(unsigned ms){
 assert(!held);assert(ms>0&&ms<=80);clock_us+=(int64_t)ms*1000;cycles++;
 if(cycles>=stop_after)longjmp(end,1);
 unsigned ts=(unsigned)cycles*5+1;regs[0x30]=ts;regs[0x31]=ts>>8;regs[0x32]=ts>>16;
 int16_t z=16384+(walk?(int)(5000*sin(cycles*0.08*2*3.141592653589793*2)):0);
 regs[0x39]=z&255;regs[0x3a]=(uint16_t)z>>8;
}
static void reset(void){
 memset(history,0,sizeof(history));memset(s_history_day,0,sizeof(s_history_day));
 memset(regs,0,sizeof(regs));memset(checkpoint,0,sizeof(checkpoint));
 regs[0]=5;regs[2]=0x20;regs[0x30]=1;regs[0x3a]=0x40;
 clock_us=0;fail_read=fail_save=walk=corrupt_config=false;held=cycles=write_count=commits=0;fail_at=-1;
 s_mutex=(void*)1;s_dev=(void*)1;s_power=(void*)1;
 s_total=s_day=s_requested_day=s_samples=s_gaps=s_read_errors=0;
 s_available=s_saved=s_save_failed=s_config_started=s_diag_requested=s_reinit_requested=s_capture_requested=false;
 watch_step_algorithm_reset();
}
static void run(unsigned samples){stop_after=(int)samples;if(!setjmp(end))worker(NULL);assert(!held);}
int main(void){
 reset();assert(configure()==0);assert(regs[2]==0x40&&regs[3]==7&&regs[6]==5&&regs[8]==0x21&&regs[9]==0x80);
 assert(configure()==0); /* warm software config */
 reset();regs[2]=0x68;regs[3]=7;regs[6]=1;regs[8]=1;regs[9]=0xd0;assert(configure()==0); /* official migration */
 reset();regs[8]=2;assert(configure()==ESP_ERR_INVALID_STATE&&write_count==0);
 reset();regs[0]=0;assert(configure()==ESP_ERR_NOT_SUPPORTED&&write_count==0);
 reset();corrupt_config=true;assert(configure()==ESP_ERR_INVALID_STATE);
 reset();fail_read=true;run(1);assert(!s_available&&!held&&write_count==0);
 reset();run(800);assert(s_total==0&&s_available&&s_samples==800);
 reset();walk=true;run(800);assert(s_total>110&&s_total<140);printf("Synthetic 128-step rhythm: %u (not accuracy evidence)\n",s_total);
 assert(commits==1); /* 60-second checkpoint, not per-sample writes */
 reset();walk=true;fail_at=50;run(100);assert(s_read_errors==1&&s_available);
 reset();s_total=50;s_reinit_requested=true;run(1);assert(s_total==50&&!s_reinit_requested);
 reset();s_total=100;s_day=20261018;s_requested_day=20261019;run(2);assert(s_day==20261019&&s_total==0);
 assert((uint32_t)history[0]==100);
 reset();s_total=100;s_requested_day=20261019;run(2);assert(s_day==20261019&&s_total==100);
 reset();s_total=100;s_day=20261018;s_requested_day=20261019;fail_save=true;run(20);
 assert(s_day==20261018&&s_total==100&&s_save_failed);
 reset();walk=true;s_total=UINT32_MAX-1;run(120);assert(s_total==UINT32_MAX);
 reset();s_day=20261018;s_total=123;save_before_restart();uint32_t value;
 assert(watch_steps_get_day(20261018,&value)==0&&value==123);
 s_day=20261019;s_total=7;assert(persist(s_day,s_total)==0);
 assert(watch_steps_get_day(20261018,&value)==0&&value==123);
 int old=commits;save_before_restart();save_before_restart();assert(commits==old);
 reset();checkpoint[0]=1;checkpoint[1]=20261018;checkpoint[2]=89;
 assert(watch_steps_init()==0&&s_total==89&&s_day==20261018);
 reset();for(unsigned i=0;i<HISTORY_DAYS;i++)assert(persist(20260000+i,i)==0);
 assert(persist(20270000,42)==0);assert(watch_steps_get_day(20260000,&value)==ESP_ERR_NOT_FOUND);
 assert(watch_steps_get_day(20260001,&value)==0&&value==1);
 puts("PASS: software configuration/migration, refusal/rollback, worker lock release, static/walking input, read failure recovery, checkpoint, date/history preservation, save failure, saturation");
}
