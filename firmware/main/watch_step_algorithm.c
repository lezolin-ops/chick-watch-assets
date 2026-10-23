#include "watch_step_algorithm.h"
#include "third_party/bangle_stepcount/stepcount.h"
#include <limits.h>
static bool have_sample;
static uint32_t last_timestamp;
static int64_t last_time;
void watch_step_algorithm_reset(void)
{
    stepcount_init();
    have_sample=false;
}
unsigned watch_step_algorithm_sample(int16_t x,int16_t y,int16_t z,
                                    uint32_t timestamp,int64_t now,bool *gap)
{
    *gap=false;
    if(!have_sample) {
        have_sample=true;last_timestamp=timestamp;last_time=now;
        *gap=true;return 0; /* explicit reset/baseline marker for replay */
    }
    if(have_sample && (((timestamp-last_timestamp)&0xffffff)==0 || ((timestamp-last_timestamp)&0xffffff)>0x800000 || now-last_time<40000 || now-last_time>120000)) {
        watch_step_algorithm_reset();
        *gap=true;
        /* Do not feed duplicate data or a discontinuous sample into the FIR. */
        last_timestamp=timestamp;last_time=now;have_sample=true;
        return 0;
    }
    have_sample=true;last_timestamp=timestamp;last_time=now;
    /* ±2g QMI data: 16384/g -> 8192/g. 64-bit sum protects every axis. */
    int32_t a=x/2,b=y/2,c=z/2;
    int64_t magnitude=(int64_t)a*a+(int64_t)b*b+(int64_t)c*c;
    if(magnitude>INT_MAX)magnitude=INT_MAX;
    return (unsigned)stepcount_new((int)magnitude);
}
