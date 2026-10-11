/** Distance-driven two-foot gait. Feet retain their illustrated shape. */
#ifndef PET_WALK_GAIT_H
#define PET_WALK_GAIT_H
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define WALK_CYCLE_MS 800
#define WALK_STRIDE_PX 24
#define WALK_STANCE_FRACTION 0.56f
#define WALK_SWING_LIFT_PX 11.0f
#define WALK_BODY_RISE_PX 4.0f
#define WALK_BODY_TILT 15.0f /* tenths of a degree */

typedef struct {
    float phase;
    float x;
    float lift;
    float angle;
    bool planted;
} walk_foot_pose_t;

/* Two weight transfers per stride; neutral at each double-support contact. */
static float walk_body_y(float distance)
{
    float rise=sinf(6.2831853f*distance/WALK_STRIDE_PX);
    return -WALK_BODY_RISE_PX*rise*rise;
}

static walk_foot_pose_t walk_foot_pose(float distance, unsigned foot)
{
    walk_foot_pose_t pose;
    pose.phase = fmodf(distance / WALK_STRIDE_PX + foot * 0.5f, 1.0f);
    pose.planted = pose.phase < WALK_STANCE_FRACTION;
    if(pose.planted) {
        /* World x = torso distance + foot x: exactly constant in stance. */
        pose.x = WALK_STRIDE_PX*WALK_STANCE_FRACTION/2 - WALK_STRIDE_PX * pose.phase;
        pose.lift = 0;
        pose.angle = 0;
    }
    else {
        float t = (pose.phase - WALK_STANCE_FRACTION) / (1.0f - WALK_STANCE_FRACTION);
        float t2 = t * t, t3 = t2 * t;
        /* Hermite swing matches the stance velocity at release and landing.
         * Slight rearward tuck, passing under the body, then forward reach. */
        float tangent = -WALK_STRIDE_PX * (1.0f - WALK_STANCE_FRACTION);
        float reach=WALK_STRIDE_PX*WALK_STANCE_FRACTION;
        pose.x = -reach/2 + reach * (3.0f * t2 - 2.0f * t3)
                 + tangent * (2.0f * t3 - 3.0f * t2 + t);
        /* Fast pickup, tucked passing foot, then a clearer forward placement.
         * Peak lift arrives before mid-swing, unlike a human pendulum step. */
        float shaped=t<0.40f ? 0.5f*t/0.40f : 0.5f+0.5f*(t-0.40f)/0.60f;
        float arc = sinf(3.14159265f * shaped);
        pose.lift = WALK_SWING_LIFT_PX * arc * arc;
        pose.angle = 210.0f*sinf(6.2831853f*t)*sinf(3.14159265f*t);
    }
    return pose;
}

/* Shift the contact shadow continuously as weight passes between feet. */
static float walk_support_x(float distance)
{
    float sum=0,weight=0;
    for(unsigned foot=0;foot<2;foot++) {
        walk_foot_pose_t pose=walk_foot_pose(distance,foot);
        if(!pose.planted) continue;
        float w=sinf(3.14159265f*pose.phase/WALK_STANCE_FRACTION);
        w*=w;
        sum+=w*pose.x; weight+=w;
    }
    return weight>0.0001f ? sum/weight : 0;
}

static float walk_travel(uint32_t elapsed, uint32_t duration, float distance)
{
    float u = (float)elapsed / duration;
    if(u >= 1.0f) return distance;
    float ramp = fminf(0.15f, 180.0f / duration);
    float progress;
    if(u < ramp) progress = u * u / (2.0f * ramp * (1.0f - ramp));
    else if(u > 1.0f - ramp) {
        float remaining = 1.0f - u;
        progress = 1.0f - remaining * remaining / (2.0f * ramp * (1.0f - ramp));
    }
    else progress = (u - ramp / 2.0f) / (1.0f - ramp);
    return distance * progress;
}
#endif
