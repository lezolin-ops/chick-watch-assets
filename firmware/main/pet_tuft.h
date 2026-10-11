/* Secondary motion from the original tuft pixels. Root/head/face stay fixed. */
#ifndef PET_TUFT_H
#define PET_TUFT_H
#include "esp_attr.h"
#ifndef PET_EXT_RAM_BSS
#define PET_EXT_RAM_BSS EXT_RAM_BSS_ATTR
#endif
typedef struct { float bend,velocity,last_x,last_y; uint32_t tick; bool ready; } tuft_motion_t;
PET_EXT_RAM_BSS static uint8_t tuft_pixels[192*192*4];
static lv_image_dsc_t tuft_image;
static const lv_image_dsc_t *tuft_original;
static unsigned tuft_base_y(const lv_image_dsc_t *image)
{
    for(unsigned y=0;y<100;y++) for(unsigned x=40;x<152;x++)
        if(image->data[y*image->header.stride+x*4+3]>180) return y+24;
    return 0;
}
static void tuft_motion_step(tuft_motion_t *motion,uint32_t now,float x,float y)
{
    if(!motion->ready) { *motion=(tuft_motion_t){.last_x=x,.last_y=y,.tick=now,.ready=true}; return; }
    motion->velocity+=-(x-motion->last_x)*1.0f+(y-motion->last_y)*1.8f;
    motion->velocity=fmaxf(-42,fminf(42,motion->velocity));
    uint32_t elapsed=LV_MIN(now-motion->tick,100);
    motion->last_x=x; motion->last_y=y; motion->tick=now;
    while(elapsed) {
        uint32_t step=LV_MIN(elapsed,10); float dt=step/1000.0f;
        float acceleration=-196*motion->bend-9*motion->velocity;
        motion->velocity+=acceleration*dt;
        motion->bend+=motion->velocity*dt;
        motion->bend=fmaxf(-2.4f,fminf(2.4f,motion->bend));
        elapsed-=step;
    }
    if(fabsf(motion->bend)<0.025f && fabsf(motion->velocity)<0.15f)
        motion->bend=motion->velocity=0;
}
static const lv_image_dsc_t *tuft_apply(const lv_image_dsc_t *source,float bend)
{
    if(source==&tuft_image) source=tuft_original; /* Never accumulate deformation. */
    tuft_original=source;
    if(fabsf(bend)<0.025f) return source;
    unsigned base=tuft_base_y(source);
    if(base<24) return source;
    unsigned top=base-24;
    tuft_image=*source; tuft_image.header.flags|=LV_IMAGE_FLAGS_MODIFIABLE; tuft_image.data=tuft_pixels;
    memcpy(tuft_pixels,source->data,source->data_size);
    for(unsigned y=top;y<base;y++) for(unsigned x=40;x<152;x++) {
        float weight=(base-y)/24.0f; weight*=weight;
        float sx=x-bend*weight;
        unsigned x0=(unsigned)floorf(sx),x1=x0+1; float t=sx-x0;
        const uint8_t *a=source->data+y*source->header.stride+x0*4;
        const uint8_t *b=source->data+y*source->header.stride+x1*4;
        uint8_t *out=tuft_pixels+y*source->header.stride+x*4;
        float a0=a[3]*(1-t),a1=b[3]*t,total=a0+a1;
        for(unsigned c=0;c<3;c++) out[c]=total>0 ? (uint8_t)lroundf((a[c]*a0+b[c]*a1)/total) : 0;
        out[3]=(uint8_t)lroundf(total);
    }
    return &tuft_image;
}
#endif
