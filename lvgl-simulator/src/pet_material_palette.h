/* Original intrinsic colours; only the added side-beak sheen is suppressed.
 * No per-pose body grading or environmental light is baked into the chick. */
#ifndef PET_MATERIAL_PALETTE_H
#define PET_MATERIAL_PALETTE_H
#include <stdlib.h>
#include <string.h>
typedef struct { const lv_image_dsc_t *source; lv_image_dsc_t image; uint8_t *pixels; } pet_color_cache_t;
static pet_color_cache_t pet_color_cache[6];
static unsigned pet_color_count;
static bool yellow_material(unsigned r,unsigned g,unsigned b)
{ return r>190 && g>155 && b<225 && r>=g && r-g<75; }
static const lv_image_dsc_t *pet_color_image(const lv_image_dsc_t *source)
{
    bool left=source==&pet_side_body_0_left || source==&pet_side_body_1_left || source==&pet_side_body_2_left;
    bool side=left || source==&pet_side_body_0_right || source==&pet_side_body_1_right || source==&pet_side_body_2_right;
    if(!side) return source;
    for(unsigned i=0;i<pet_color_count;i++)
        if(pet_color_cache[i].source==source) return &pet_color_cache[i].image;
    if(pet_color_count>=6) return source;
    uint8_t *data=malloc(source->data_size);
    if(!data) return source;
    memcpy(data,source->data,source->data_size);
    /* Restrict matte material to the side bill, never body, eye or contour. */
    for(unsigned y=70;y<118;y++) for(unsigned x=130;x<176;x++) {
        unsigned xx=left ? 191-x : x;
        uint8_t *p=data+y*source->header.stride+4*xx;
        if(p[3]>0 && p[2]>225 && p[1]>65 && p[1]<190 && p[0]<125 && p[2]-p[1]>80) {
            p[2]=255; p[1]=112; p[0]=40;
        }
    }
    pet_color_cache_t *entry=&pet_color_cache[pet_color_count++];
    entry->source=source; entry->pixels=data; entry->image=*source; entry->image.data=data;
    return &entry->image;
}
#endif
