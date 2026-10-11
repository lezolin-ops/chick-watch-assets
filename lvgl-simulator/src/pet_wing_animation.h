/* Unevenly timed, genuinely drawn shape changes, linked to walking distance. */
#ifndef PET_WING_ANIMATION_H
#define PET_WING_ANIMATION_H
static const lv_image_dsc_t * const wing_animation[2][6]={
    {&pet_wing_frame_0_right,&pet_wing_frame_1_right,&pet_wing_frame_2_right,&pet_wing_frame_3_right,&pet_wing_frame_4_right,&pet_wing_frame_5_right},
    {&pet_wing_frame_0_left,&pet_wing_frame_1_left,&pet_wing_frame_2_left,&pet_wing_frame_3_left,&pet_wing_frame_4_left,&pet_wing_frame_5_left}
};
static unsigned wing_walk_frame(float distance)
{
    float phase=fmodf(distance/WALK_STRIDE_PX,1.0f);
    if(phase<0.08f || phase>=0.82f) return 0;
    if(phase<0.24f) return 1;
    if(phase<0.40f) return 2;
    if(phase<0.54f) return 3;
    if(phase<0.66f) return 4;
    return 5;
}
static unsigned wing_settle_frame(unsigned start,uint32_t elapsed)
{
    if(start==0) return 0;
    if(start<=3) return elapsed<60 ? 4 : elapsed<120 ? 5 : 0;
    return elapsed<90 ? 5 : 0;
}
#endif
