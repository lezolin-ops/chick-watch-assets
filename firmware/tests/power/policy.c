#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../../main/watch_power.c"
static unsigned pm_live;
static int fail_create_type, nvs_fail, commit_fail;
static uint8_t saved_off, pending_off;
static watch_wifi_status_t wifi = {.enabled = true};
const char *esp_err_to_name(esp_err_t err){(void)err;return "test";}
int64_t esp_timer_get_time(void){return 1000000;}
esp_err_t gpio_config(const gpio_config_t *c){(void)c;return ESP_OK;}
esp_err_t gpio_wakeup_enable(int pin,int level){(void)pin;(void)level;return ESP_OK;}
esp_err_t gpio_sleep_sel_dis(int pin){(void)pin;return ESP_OK;}
esp_err_t esp_sleep_enable_gpio_wakeup(void){return ESP_OK;}
esp_err_t lvgl_port_stop(void){return ESP_OK;}
esp_err_t esp_pm_configure(const esp_pm_config_t *c){assert(c->max_freq_mhz==240&&c->min_freq_mhz==80);return ESP_OK;}
esp_err_t esp_pm_lock_create(int type,int arg,const char *name,esp_pm_lock_handle_t *out){
 (void)arg;(void)name;if(type==fail_create_type)return ESP_FAIL;
 *out=calloc(1,sizeof(**out));(*out)->type=type;pm_live++;return ESP_OK;
}
esp_err_t esp_pm_lock_acquire(esp_pm_lock_handle_t p){assert(!p->held);p->held++;return ESP_OK;}
esp_err_t esp_pm_lock_release(esp_pm_lock_handle_t p){assert(p->held==1);p->held--;return ESP_OK;}
esp_err_t esp_pm_lock_delete(esp_pm_lock_handle_t p){assert(!p->held);pm_live--;free(p);return ESP_OK;}
SemaphoreHandle_t xSemaphoreCreateMutex(void){pthread_mutex_t *p=malloc(sizeof(*p));pthread_mutex_init(p,NULL);return p;}
int xSemaphoreTake(SemaphoreHandle_t p,int t){(void)t;return pthread_mutex_lock(p);}
int xSemaphoreGive(SemaphoreHandle_t p){return pthread_mutex_unlock(p);}
esp_err_t nvs_open(const char *n,int m,nvs_handle_t *h){(void)m;assert(!strcmp(n,"pet_power"));*h=1;return nvs_fail;}
esp_err_t nvs_get_u8(nvs_handle_t h,const char *k,uint8_t *v){(void)h;assert(!strcmp(k,"sleep_wifi_off"));*v=saved_off;return ESP_OK;}
esp_err_t nvs_set_u8(nvs_handle_t h,const char *k,uint8_t v){(void)h;(void)k;pending_off=v;return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h){(void)h;if(commit_fail)return commit_fail;saved_off=pending_off;return ESP_OK;}
void nvs_close(nvs_handle_t h){(void)h;}
void watch_wifi_status(watch_wifi_status_t *s){*s=wifi;}
esp_err_t watch_wifi_set_enabled(bool on){wifi.enabled=on;return ESP_OK;}
int main(void){
 lv_init();assert(watch_power_init()==ESP_OK);
 assert(!watch_power_sleep_wifi_off());
 assert(watch_power_try_stop_network()==ESP_ERR_INVALID_STATE&&wifi.enabled);
 assert(watch_power_set_sleep_wifi_off(true)==ESP_OK&&saved_off==1);
 watch_power_client_t a,b;
 assert(watch_power_client_create("download",WATCH_POWER_NETWORK|WATCH_POWER_CPU|WATCH_POWER_AWAKE,&a)==ESP_OK);
 assert(watch_power_client_create("background",WATCH_POWER_NETWORK,&b)==ESP_OK);
 assert(watch_power_client_acquire(a)==ESP_OK);
 assert(watch_power_client_acquire(a)==ESP_ERR_INVALID_STATE);
 assert(watch_power_client_acquire(b)==ESP_OK);
 assert(watch_power_client_delete(a)==ESP_ERR_INVALID_STATE);
 watch_power_set_blanked(true);assert(!s_locked&&a->awake->held==1);
 assert(watch_power_try_stop_network()==ESP_ERR_INVALID_STATE&&wifi.enabled);
 assert(watch_power_try_stop_offline_network()==ESP_ERR_INVALID_STATE&&wifi.enabled);
 assert(watch_power_client_release(a)==ESP_OK);
 assert(watch_power_client_release(a)==ESP_ERR_INVALID_STATE);
 assert(watch_power_try_stop_network()==ESP_ERR_INVALID_STATE&&wifi.enabled);
 assert(watch_power_client_release(b)==ESP_OK);
 assert(watch_power_try_stop_network()==ESP_OK&&!wifi.enabled);
 assert(watch_power_client_acquire(b)==ESP_ERR_INVALID_STATE);
 assert(watch_power_client_delete(a)==ESP_OK&&watch_power_client_delete(b)==ESP_OK);
 nvs_fail=ESP_FAIL;assert(watch_power_set_sleep_wifi_off(false)==ESP_FAIL&&watch_power_sleep_wifi_off());
 nvs_fail=0;commit_fail=ESP_FAIL;
 assert(watch_power_set_sleep_wifi_off(false)==ESP_FAIL&&watch_power_sleep_wifi_off()&&saved_off==1);
 commit_fail=0;assert(watch_power_set_sleep_wifi_off(false)==ESP_OK);
 wifi.enabled=true;assert(watch_power_try_stop_network()==ESP_ERR_INVALID_STATE&&wifi.enabled);
 unsigned live=pm_live;fail_create_type=ESP_PM_NO_LIGHT_SLEEP;
 assert(watch_power_client_create("failed",WATCH_POWER_CPU|WATCH_POWER_AWAKE,&a)==ESP_FAIL&&a==NULL);
 assert(pm_live==live);watch_power_set_blanked(false);assert(s_locked);
 assert(watch_power_try_stop_offline_network()==ESP_OK&&!wifi.enabled);
 puts("PASS: default retains Wi-Fi, saved policy, two app leases prevent stop/sleep, duplicate calls and failure cleanup");
 return 0;
}
