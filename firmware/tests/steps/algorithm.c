#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include "watch_step_algorithm.h"
#include "third_party/bangle_stepcount/stepcount.h"
extern bool gate_open;
extern int active_sample_count;
int main(void) {
 bool gap;watch_step_algorithm_reset();
 assert(watch_step_algorithm_sample(0,0,16384,0xfffffa,0,&gap)==0&&gap);
 assert(watch_step_algorithm_sample(0,0,16384,1,80000,&gap)==0&&!gap); /* timestamp wraps */
 assert(watch_step_algorithm_sample(0,0,16384,1,160000,&gap)==0&&gap); /* duplicate */
 assert(watch_step_algorithm_sample(0,0,16384,0,240000,&gap)==0&&gap); /* device time backwards */
 assert(watch_step_algorithm_sample(0,0,16384,5,800000,&gap)==0&&gap); /* missed sampling */
 assert(watch_step_algorithm_sample(0,0,16384,10,801000,&gap)==0&&gap); /* catch-up burst */
 for(int i=0;i<10000;i++) {
   int16_t x=(i%2)?INT16_MIN:INT16_MAX;
   (void)watch_step_algorithm_sample(x,x,x,(uint32_t)i+20,900000+(int64_t)i*80000,&gap);
 }
 gate_open=true;active_sample_count=3;watch_step_algorithm_reset();
 assert(!gate_open&&active_sample_count==0);
 for(unsigned axis=0;axis<3;axis++) {
   watch_step_algorithm_reset();unsigned total=0;
   for(int i=0;i<1000;i++) {
     int16_t xyz[3]={0};xyz[axis]=16384;
     total+=watch_step_algorithm_sample(xyz[0],xyz[1],xyz[2],i,(int64_t)i*80000,&gap);
   }
   assert(total==0);
 }
 /* Reset must produce identical replay, including pending-step/gate state. */
 unsigned result[2]={0};
 for(unsigned run=0;run<2;run++) {
   watch_step_algorithm_reset();
   for(int i=0;i<800;i++) {
     int16_t z=16384+(int)(5000*sin(i*.08*2*3.141592653589793*2));
     result[run]+=watch_step_algorithm_sample(0,0,z,i,(int64_t)i*80000,&gap);
   }
 }
 assert(result[0]==result[1]&&result[0]==128);
 puts("PASS: timestamp wrap/rollback, duplicate/gap/burst rejection, extreme axes under UBSan, complete reset, orientation-independent static input, deterministic replay");
}
