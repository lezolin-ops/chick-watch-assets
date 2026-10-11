/* Original body/silhouette stays intact. Only erase the baked internal wing
 * marks beneath the independent wing; never replace or animate the rump. */
#ifndef PET_WALK_EXPRESSION_H
#define PET_WALK_EXPRESSION_H
#include "esp_attr.h"
#ifndef PET_EXT_RAM_BSS
#define PET_EXT_RAM_BSS EXT_RAM_BSS_ATTR
#endif
PET_EXT_RAM_BSS static uint8_t walk_expression_pixels[192*192*4];
static lv_image_dsc_t walk_expression_image;
static const lv_image_dsc_t *walk_expression_source;
static int walk_expression_direction;
static const lv_image_dsc_t *walk_expressive_body(const lv_image_dsc_t *source,
                                                float distance,int direction)
{
    (void)distance;
    if(walk_expression_source==source && walk_expression_direction==direction)
        return &walk_expression_image;
    walk_expression_source=source;
    walk_expression_direction=direction;
    walk_expression_image=*source;
    walk_expression_image.header.flags|=LV_IMAGE_FLAGS_MODIFIABLE;
    walk_expression_image.data=walk_expression_pixels;
    memcpy(walk_expression_pixels,source->data,source->data_size);
    /* Soft atlas already has a clean wingless interior. Keep its continuous
     * shading and stable rump silhouette rather than painting a flat patch. */
    return &walk_expression_image;
}
#endif
