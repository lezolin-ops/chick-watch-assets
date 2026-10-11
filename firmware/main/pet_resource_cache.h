/* Lossless storage overlay -> bounded, stable-address ARGB8888 resource views.
 * Call only under the LVGL task/lock, like the existing animation pixel code.
 * Large character poses use an LRU; scene/small assets stay resident. */
#ifndef PET_RESOURCE_CACHE_H
#define PET_RESOURCE_CACHE_H
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#if PET_USE_PACKED_ASSETS
#include "src/libs/lz4/lz4.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_log.h"
#endif
#ifndef PET_RESOURCE_BUDGET_BYTES
#define PET_RESOURCE_BUDGET_BYTES (3U*1024U*1024U)
#endif
#define PET_RESOURCE_BODY_LIMIT 6U
#define PET_RESOURCE_ENTRY_LIMIT 192U
typedef struct {
    const lv_image_dsc_t *source;
    lv_image_dsc_t image;
    uint32_t last;
    bool body;
} pet_resource_entry_t;
static pet_resource_entry_t pet_resources[PET_RESOURCE_ENTRY_LIMIT];
static unsigned pet_resource_count,pet_resource_body_count;
static uint32_t pet_resource_clock;
static size_t pet_resource_bytes,pet_resource_peak;
static unsigned pet_resource_loads,pet_resource_hits,pet_resource_evictions;
static pet_resource_entry_t *pet_resource_current,*pet_resource_previous;

static void pet_resource_require(bool condition,const char *message)
{
    /* Firmware disables some LVGL assertions; safety checks must still run. */
    if(!condition) {
#ifdef ESP_PLATFORM
        ESP_LOGE("pet_resources","%s",message);
#else
        LV_LOG_ERROR("%s",message);
#endif
        abort();
    }
}

static uint32_t pet_resource_u32(const uint8_t *p)
{ return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static void *pet_resource_alloc(size_t bytes)
{
#ifdef ESP_PLATFORM
    return heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
    return malloc(bytes); /* Host mirrors external pixel RAM, not the UI heap. */
#endif
}
static void pet_resource_free(void *pixels)
{
#ifdef ESP_PLATFORM
    heap_caps_free(pixels);
#else
    free(pixels);
#endif
}
static void pet_resource_unload(pet_resource_entry_t *entry)
{
    if(!entry->image.data) return;
    lv_image_cache_drop(&entry->image);
    pet_resource_bytes-=entry->image.data_size;
    if(entry->body) pet_resource_body_count--;
    pet_resource_free((void *)entry->image.data);
    entry->image.data=NULL;
}
static bool pet_resource_evict(void)
{
    pet_resource_entry_t *victim=NULL;
    for(unsigned i=0;i<pet_resource_count;i++) {
        pet_resource_entry_t *e=&pet_resources[i];
        if(!e->body || !e->image.data || e==pet_resource_current || e==pet_resource_previous) continue;
        if(!victim || e->last<victim->last) victim=e;
    }
    if(!victim) return false;
    pet_resource_unload(victim);pet_resource_evictions++;return true;
}
static const lv_image_dsc_t *pet_resource_image(const lv_image_dsc_t *source)
{
    if(!source) return NULL;
    pet_resource_entry_t *entry=NULL;
    for(unsigned i=0;i<pet_resource_count;i++) {
        if(&pet_resources[i].image==source) { entry=&pet_resources[i];source=entry->source;break; }
    }
    if(!(source->header.flags&LV_IMAGE_FLAGS_COMPRESSED)) return source;
    if(!entry) for(unsigned i=0;i<pet_resource_count;i++)
        if(pet_resources[i].source==source) {entry=&pet_resources[i];break;}
    if(!entry) {
        pet_resource_require(pet_resource_count<PET_RESOURCE_ENTRY_LIMIT,"Resource registry exhausted");
        entry=&pet_resources[pet_resource_count++];
        entry->source=source;entry->image=*source;entry->image.data=NULL;
        /* Generator tags pose resources; cache policy does not guess dimensions. */
        entry->body=(source->header.flags&LV_IMAGE_FLAGS_USER1)!=0;
    }
    entry->last=++pet_resource_clock;
    if(!entry->image.data) {
        pet_resource_require(source->data_size>=12,"Bad compressed resource header");
        uint32_t method=pet_resource_u32(source->data)&15;
        uint32_t input=pet_resource_u32(source->data+4),output=pet_resource_u32(source->data+8);
        pet_resource_require(method==LV_IMAGE_COMPRESS_LZ4 && input==source->data_size-12,"Bad LZ4 resource");
        pet_resource_require(output==source->header.stride*source->header.h && output<=INT_MAX,"Bad resource pixel size");
        while((entry->body && pet_resource_body_count>=PET_RESOURCE_BODY_LIMIT) ||
              pet_resource_bytes+output>PET_RESOURCE_BUDGET_BYTES) {
            bool evicted=pet_resource_evict();
            pet_resource_require(evicted,"Resource memory budget exceeded; release old scene resources");
        }
        uint8_t *pixels=pet_resource_alloc(output);
        pet_resource_require(pixels!=NULL,"No PSRAM for resource decode");
        int decoded=LZ4_decompress_safe((const char *)source->data+12,(char *)pixels,(int)input,(int)output);
        pet_resource_require(decoded==(int)output,"LZ4 resource decode failed");
        entry->image.header.flags&=~(LV_IMAGE_FLAGS_COMPRESSED|LV_IMAGE_FLAGS_USER1);
        entry->image.data=pixels;entry->image.data_size=output;
        pet_resource_bytes+=output;
        if(pet_resource_bytes>pet_resource_peak) pet_resource_peak=pet_resource_bytes;
        if(entry->body) pet_resource_body_count++;
        pet_resource_loads++;
    }
    else pet_resource_hits++;
    if(entry->body && entry!=pet_resource_current) {
        pet_resource_previous=pet_resource_current;pet_resource_current=entry;
    }
    return &entry->image;
}
/* After deleting old scene objects under the LVGL lock, future scene switches
 * may release their resident resources. Stable descriptors can be loaded again. */
static inline void pet_resource_clear(void)
{
    for(unsigned i=0;i<pet_resource_count;i++) pet_resource_unload(&pet_resources[i]);
    pet_resource_current=pet_resource_previous=NULL;
}
#else
static const lv_image_dsc_t *pet_resource_image(const lv_image_dsc_t *source) {return source;}
#endif
#endif
