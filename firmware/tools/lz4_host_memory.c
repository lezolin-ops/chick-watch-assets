/* Host-only memory glue for LVGL's bundled freestanding LZ4 codec.
 * No device allocator or LVGL initialisation is needed for this codec. */
#include <stddef.h>
#include <string.h>
void *lv_memcpy(void *dest,const void *src,size_t n) { return memcpy(dest,src,n); }
void *lv_memmove(void *dest,const void *src,size_t n) { return memmove(dest,src,n); }
void *lv_memset(void *dest,int value,size_t n) { return memset(dest,value,n); }
