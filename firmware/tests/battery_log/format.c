#include <assert.h>
#include "watch_battery_log_format.h"
int main(void){batlog_chunk_t c={.magic=BATLOG_MAGIC,.version=1,.record_size=24,.start_seq=512,.count=64};
for(unsigned i=0;i<64;i++)c.records[i].seq=512+i;
c.crc=batlog_crc(c.records,64*sizeof(batlog_record_t));assert(batlog_valid(&c,batlog_size(&c),0));assert(!batlog_valid(&c,batlog_size(&c),1));assert(!batlog_valid(&c,19,0));c.records[3].percent=4;assert(!batlog_valid(&c,batlog_size(&c),0));c.count=65;assert(!batlog_valid(&c,sizeof(c),0));return 0;}
