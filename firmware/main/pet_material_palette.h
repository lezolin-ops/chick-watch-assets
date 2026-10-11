/* Unified authored soft materials; future environmental light must be shared
 * by every pose, not applied as a side-only material correction. */
#ifndef PET_MATERIAL_PALETTE_H
#define PET_MATERIAL_PALETTE_H
#include <stdlib.h>
#include <string.h>
typedef struct { const lv_image_dsc_t *source; lv_image_dsc_t image; uint8_t *pixels; } pet_color_cache_t;
static bool yellow_material(unsigned r,unsigned g,unsigned b)
{ return r>190 && g>155 && b<225 && r>=g && r-g<75; }
static const lv_image_dsc_t *pet_color_image(const lv_image_dsc_t *source)
{
    /* All soft-style poses share authored matte materials; no pose grading. */
    return pet_resource_image(source);
}
#endif
