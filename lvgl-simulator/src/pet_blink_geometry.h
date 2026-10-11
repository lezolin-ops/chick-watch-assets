/* Generated eye geometry for native LVGL eyelids. */
typedef struct { int16_t x, y, w, h; uint32_t color; } eye_region_t;
typedef struct { uint8_t count; eye_region_t eyes[2]; } eye_pose_t;
static const eye_pose_t daily_eyes[12] = {
    { 2, { { 114, 104, 19, 19, 0xFCE43F }, { 58, 104, 19, 19, 0xFCE43F } } },
    { 2, { { 114, 92, 19, 19, 0xFCE33F }, { 59, 92, 20, 19, 0xFCE33D } } },
    { 2, { { 114, 75, 18, 19, 0xFCE43F }, { 61, 75, 19, 19, 0xFCE43F } } },
    { 2, { { 114, 71, 18, 19, 0xFCE33D }, { 61, 71, 18, 19, 0xFCE33D } } },
    { 2, { { 79, 76, 19, 20, 0xFCE43E }, { 130, 73, 15, 18, 0xFCE13C } } },
    { 2, { { 103, 75, 19, 20, 0xFCE33D }, { 143, 72, 11, 17, 0xFCDB3C } } },
    { 1, { { 119, 74, 18, 19, 0xFCE43E } } },
    { 1, { { 120, 74, 18, 20, 0xFCE33E } } },
    { 1, { { 122, 75, 18, 20, 0xFCE43F } } },
    { 1, { { 122, 75, 18, 20, 0xFCE33F } } },
    { 2, { { 81, 73, 20, 20, 0xFCE43F }, { 132, 71, 15, 18, 0xFCE440 } } },
    { 2, { { 114, 110, 18, 19, 0xFCE33F }, { 59, 110, 19, 19, 0xFCE33D } } },
};
static const eye_pose_t illustrated_walk_eyes[4] = {
    { 1, { { 123, 72, 18, 20, 0xFCDA35 } } },
    { 1, { { 123, 72, 18, 20, 0xFCDA36 } } },
    { 1, { { 123, 71, 19, 20, 0xFCD935 } } },
    { 1, { { 123, 71, 18, 20, 0xFCD935 } } },
};
static const eye_pose_t hungry_eyes = { 2, { { 106, 83, 23, 15, 0xFCDD42 }, { 53, 91, 21, 16, 0xFCDD42 } } };
static const eye_pose_t rig_body_eyes = { 1, { { 120, 73, 17, 19, 0xFCD83B } } };
