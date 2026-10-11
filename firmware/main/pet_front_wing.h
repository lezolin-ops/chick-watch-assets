/* Front wing drawing patches: the original face, centre and feet never move. */
#ifndef PET_FRONT_WING_H
#define PET_FRONT_WING_H
#include "esp_attr.h"
#ifndef PET_EXT_RAM_BSS
#define PET_EXT_RAM_BSS EXT_RAM_BSS_ATTR
#endif
static const lv_image_dsc_t *const front_flap_patch[2][6]={
    {&pet_front_flap_patch_0_left,&pet_front_flap_patch_1_left,&pet_front_flap_patch_2_left,&pet_front_flap_patch_3_left,&pet_front_flap_patch_4_left,&pet_front_flap_patch_5_left},
    {&pet_front_flap_patch_0_right,&pet_front_flap_patch_1_right,&pet_front_flap_patch_2_right,&pet_front_flap_patch_3_right,&pet_front_flap_patch_4_right,&pet_front_flap_patch_5_right}
};
PET_EXT_RAM_BSS static uint8_t front_wing_body_pixels[192*192*4];
static lv_image_dsc_t front_wing_body;
static const lv_image_dsc_t *front_wing_body_source;
static unsigned front_wing_body_frame;
static bool is_cheek(const uint8_t *p)
{ return p[2]>220 && p[1]>85 && p[1]<190 && p[0]>85; }
static void match_wing_outline(uint8_t *p)
{
    /* Preserve authored soft overlap shading; do not reintroduce contours. */
    (void)p;
}
static pet_color_cache_t wing_material_cache[12];
static unsigned wing_material_count;
static const lv_image_dsc_t *wing_frame_material(const lv_image_dsc_t *source)
{
    for(unsigned i=0;i<wing_material_count;i++) if(wing_material_cache[i].source==source) return &wing_material_cache[i].image;
    if(wing_material_count>=12) return source;
    uint8_t *pixels=malloc(source->data_size); if(!pixels) return source;
    memcpy(pixels,source->data,source->data_size);
    for(unsigned i=0;i<source->data_size;i+=4) if(pixels[i+3]) match_wing_outline(pixels+i);
    pet_color_cache_t *entry=&wing_material_cache[wing_material_count++];
    entry->source=source; entry->image=*source; entry->pixels=pixels; entry->image.data=pixels;
    return &entry->image;
}
static const lv_image_dsc_t *front_body_pose(const lv_image_dsc_t *source,bool seated,unsigned frame)
{
    if(frame==0) return source; /* Exact original folded pose, no size jump. */
    if(front_wing_body_source==source && front_wing_body_frame==frame) return &front_wing_body;
    front_wing_body_source=source; front_wing_body_frame=frame;
    front_wing_body=*source; front_wing_body.header.flags|=LV_IMAGE_FLAGS_MODIFIABLE;
    front_wing_body.data=front_wing_body_pixels;
    memcpy(front_wing_body_pixels,source->data,source->data_size);
    int offset=seated ? 32 : 0;
    for(unsigned side=0;side<2;side++) {
        const lv_image_dsc_t *patch=front_flap_patch[side][frame];
        int x0=side ? 132 : 10;
        for(int py=0;py<68;py++) for(int px=0;px<50;px++) {
            int x=x0+px,y=80+offset+py;
            uint8_t *out=front_wing_body_pixels+y*source->header.stride+x*4;
            const uint8_t *part=patch->data+py*patch->header.stride+px*4;
            int cx=side ? 191-x : x;
            if(py<18 && out[3]>80) continue; /* Foreground head silhouette stays original. */
            if(is_cheek(out) || is_cheek(part)) continue;
            if(seated && y>=150 && cx>=44) continue; /* Entire foot rectangles, including AA fringes. */
            float mix=fminf(1,fminf(py/6.0f,(67-py)/6.0f));
            mix*=fminf(1,fmaxf(0,(52-cx)/4.0f));
            uint8_t colour[4]; memcpy(colour,part,4); match_wing_outline(colour);
            float a0=out[3]*(1-mix),a1=colour[3]*mix,total=a0+a1;
            for(int c=0;c<3;c++) out[c]=total>0 ? (uint8_t)lroundf((out[c]*a0+colour[c]*a1)/total) : 0;
            out[3]=(uint8_t)lroundf(total);
        }
    }
    return &front_wing_body;
}
#endif
