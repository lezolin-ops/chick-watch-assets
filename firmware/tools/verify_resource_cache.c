/* Host-side verification of the actual firmware lossless decoding/cache code. */
#include "lvgl/lvgl.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "pet_packed_assets.c"
#include "../main/pet_resource_cache.h"
static const struct { const char *name; const lv_image_dsc_t *source; uint32_t crc; } assets[]={
#include "pet_resource_test_entries.h"
};
static uint32_t crc32(const uint8_t *data,size_t length)
{
    uint32_t crc=~0U;
    for(size_t i=0;i<length;i++) {
        crc^=data[i];
        for(unsigned bit=0;bit<8;bit++) crc=(crc>>1)^(0xEDB88320U&-(crc&1));
    }
    return ~crc;
}
int main(void)
{
    lv_init();
    for(unsigned i=0;i<sizeof(assets)/sizeof(assets[0]);i++) {
        const lv_image_dsc_t *image=pet_resource_image(assets[i].source);
        assert(!(image->header.flags&LV_IMAGE_FLAGS_COMPRESSED));
        assert(image->data_size==image->header.stride*image->header.h);
        assert(crc32(image->data,image->data_size)==assets[i].crc);
        pet_resource_clear();
    }
    puts("PASS: every native-decoded asset matches the original BGRA bytes/CRC32.");
    const lv_image_dsc_t *pose_sources[]={&pet_daily_right_0,&pet_daily_right_1,&pet_daily_right_2,
        &pet_daily_right_3,&pet_daily_right_4,&pet_daily_right_5,&pet_daily_left_0,&pet_daily_left_1,
        &pet_daily_left_2,&pet_daily_left_3,&pet_daily_left_4,&pet_daily_left_5};
    const lv_image_dsc_t *first=pet_resource_image(pose_sources[0]);
    for(unsigned loop=0;loop<100;loop++) for(unsigned i=0;i<12;i++) {
        const lv_image_dsc_t *image=pet_resource_image(pose_sources[i]);
        assert(image->data_size==192*192*4);
        assert(pet_resource_body_count<=PET_RESOURCE_BODY_LIMIT);
        assert(pet_resource_bytes<=PET_RESOURCE_BUDGET_BYTES);
    }
    assert(pet_resource_image(pose_sources[0])==first); /* Stable descriptor after eviction/reload. */
    assert(pet_resource_evictions>100);
    printf("PASS: 1,200 pose visits, bounded six-pose LRU; %u evictions, peak %zu bytes.\n",pet_resource_evictions,pet_resource_peak);
    pet_resource_clear();assert(pet_resource_bytes==0);
    return 0;
}
