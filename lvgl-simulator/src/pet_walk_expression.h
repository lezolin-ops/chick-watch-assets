/* Original body/silhouette stays intact. Only erase the baked internal wing
 * marks beneath the independent wing; never replace or animate the rump. */
#ifndef PET_WALK_EXPRESSION_H
#define PET_WALK_EXPRESSION_H
static uint8_t walk_expression_pixels[192*192*4];
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
    for(int y=101;y<147;y++) for(int cx=29;cx<80;cx++) {
        int x=direction<0 ? 191-cx : cx;
        uint8_t *out=walk_expression_pixels+y*source->header.stride+4*x;
        bool edge=false;
        for(int dy=-3;dy<=3;dy++) for(int dx=-3;dx<=3;dx++)
            if(source->data[(y+dy)*source->header.stride+4*(x+dx)+3]<200) edge=true;
        if(edge || out[3]<200) continue;
        int skin_x=direction<0 ? 191-90 : 90;
        const uint8_t *skin=source->data+y*source->header.stride+4*skin_x;
        /* Preserve alpha and the entire original outer contour byte-for-byte. */
        memcpy(out,skin,3);
    }
    return &walk_expression_image;
}
#endif
