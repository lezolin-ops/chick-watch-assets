/* Generated eye geometry for native LVGL eyelids. */
typedef struct { int16_t x, y, w, h; uint32_t color; } eye_region_t;
typedef struct { uint8_t count; eye_region_t eyes[2]; } eye_pose_t;
static const eye_pose_t daily_eyes[12] = {
    { 2, { { 60, 104, 17, 19, 0xFCD535 }, { 114, 104, 18, 19, 0xFCD535 } } },
    { 2, { { 61, 92, 17, 19, 0xFCD434 }, { 115, 92, 17, 19, 0xFCD435 } } },
    { 2, { { 61, 75, 17, 19, 0xFCD534 }, { 113, 75, 18, 19, 0xFCD535 } } },
    { 2, { { 60, 70, 18, 18, 0xFCD434 }, { 113, 71, 18, 18, 0xFCD535 } } },
    { 2, { { 81, 74, 18, 18, 0xFCD434 }, { 131, 71, 14, 18, 0xFCD435 } } },
    { 2, { { 105, 73, 17, 19, 0xFCD435 }, { 144, 71, 10, 16, 0xFBD336 } } },
    { 1, { { 120, 73, 16, 18, 0xFCD434 } } },
    { 1, { { 123, 76, 15, 18, 0xFCD434 } } },
    { 1, { { 123, 74, 17, 19, 0xFCD535 } } },
    { 1, { { 124, 75, 17, 20, 0xFCD434 } } },
    { 2, { { 83, 73, 18, 19, 0xFCD434 }, { 133, 71, 13, 18, 0xFBD435 } } },
    { 2, { { 60, 111, 17, 19, 0xFCD536 }, { 114, 111, 17, 19, 0xFCD435 } } },
};
static const eye_pose_t illustrated_walk_eyes[4] = {
    { 1, { { 123, 72, 18, 20, 0xFCDA35 } } },
    { 1, { { 123, 72, 18, 20, 0xFCDA36 } } },
    { 1, { { 123, 71, 19, 20, 0xFCD935 } } },
    { 1, { { 123, 71, 18, 20, 0xFCD935 } } },
};
static const eye_pose_t hungry_eyes = { 2, { { 106, 81, 23, 15, 0xFDD33C }, { 54, 90, 20, 16, 0xFCD23B } } };
static const eye_pose_t rig_body_eyes = { 1, { { 120, 73, 17, 19, 0xFCD83B } } };
