#include <assert.h>
#include "watch_wifi_policy.h"
int main(void){
assert(!watch_wifi_offline_due(true,true,false,0,WATCH_WIFI_OFFLINE_US-1));
assert(watch_wifi_offline_due(true,true,false,0,WATCH_WIFI_OFFLINE_US));
assert(!watch_wifi_offline_due(true,false,false,0,WATCH_WIFI_OFFLINE_US));
assert(!watch_wifi_offline_due(true,true,true,0,WATCH_WIFI_OFFLINE_US));
assert(!watch_wifi_offline_due(false,true,false,0,WATCH_WIFI_OFFLINE_US));
assert(!watch_wifi_offline_due(true,true,false,-1,WATCH_WIFI_OFFLINE_US));
assert(!watch_wifi_offline_due(true,true,false,100,99));
assert(!watch_wifi_offline_due(true,true,false,WATCH_WIFI_OFFLINE_US,WATCH_WIFI_OFFLINE_US+1));
watch_wifi_retry_t retry={0};
assert(watch_wifi_retry_due(&retry,0));
const unsigned delays[]={30,60,120,300,300,300};
int64_t now=0;
for(unsigned i=0;i<6;i++){
 assert(watch_wifi_retry_failed(&retry,now)==delays[i]);
 assert(!watch_wifi_retry_due(&retry,now+(int64_t)delays[i]*1000000-1));
 now+=(int64_t)delays[i]*1000000;
 assert(watch_wifi_retry_due(&retry,now));
}
/* Manual connection and successful connection restart the sequence. */
watch_wifi_retry_reset(&retry);assert(watch_wifi_retry_due(&retry,now));
assert(watch_wifi_retry_failed(&retry,now)==30);
/* Offline shutoff has its own deadline; a distant retry cannot defer it. */
retry.next_us=WATCH_WIFI_OFFLINE_US+300000000;
assert(watch_wifi_offline_due(true,true,false,0,WATCH_WIFI_OFFLINE_US));
assert(!watch_wifi_retry_due(&retry,WATCH_WIFI_OFFLINE_US));
return 0;}
