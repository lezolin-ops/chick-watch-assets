/* Offscreen LVGL verification of illustrated walking and independent blinking.
 * Includes the implementation so the animation's exact poses can be inspected.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/pet.c"

static uint8_t draw_buffer[410 * 502 * 4];
static uint8_t screen_buffer[410 * 502 * 4];
static uint8_t stable_body[410 * 130 * 4];
static uint8_t mouth_baseline[410 * 502 * 4];

static unsigned alpha_at(const lv_image_dsc_t *image, int x, int y)
{
    if(x < 0 || y < 0 || x >= image->header.w || y >= image->header.h) return 0;
    return image->data[y * image->header.stride + x * 4 + 3];
}

static void verify_cloud_art(void)
{
    const lv_image_dsc_t *clouds[2]={&pet_scene_cloud_near,&pet_scene_cloud_far};
    assert(clouds[0]->header.w>clouds[1]->header.w*1.8f);
    for(unsigned n=0;n<2;n++) {
        const lv_image_dsc_t *cloud=clouds[n];
        unsigned opaque=0,soft=0,white=0,blue=0;
        for(unsigned i=0;i<cloud->data_size;i+=4) {
            const uint8_t *p=cloud->data+i;
            if(p[3]>220) {
                opaque++;
                if(p[2]>240 && p[1]>240) white++;
                if(p[0]>p[2]+10 && p[0]>230) blue++;
            }
            if(p[3]>10 && p[3]<220) soft++;
        }
        assert(opaque>100 && soft>10 && white>30 && blue>30);
        assert(alpha_at(cloud,0,0)==0 && alpha_at(cloud,cloud->header.w-1,0)==0);
        assert(scene_cloud_x(0,n)>=0 && scene_cloud_x(0,n)+cloud->header.w<=410);
        uint32_t wrap=PET_CLOUD_PERIOD_MS-cloud_phase_offset[n];
        assert(scene_cloud_x(wrap-1,n)>410 && scene_cloud_x(wrap,n)==-160);
        assert(scene_cloud_x(1000,n)>scene_cloud_x(0,n));
    }
    puts("PASS: distinct painted near/far clouds, milk-white/blue shading, soft alpha, complete initial layout and offscreen wrapping.");
}

static void verify_attachment(void)
{
    unsigned minimum = 100000;
    for(unsigned sample = 0; sample < 360; sample++) {
        float distance = WALK_STRIDE_PX * sample / 360.0f;
        for(int direction = -1; direction <= 1; direction += 2) {
            for(unsigned foot = 0; foot < 2; foot++) {
                walk_foot_pose_t pose = walk_foot_pose(distance, foot);
                const lv_image_dsc_t *image = foot_images[direction < 0][foot];
                int px = direction < 0 ? 30 : 18, py = 26;
                int ankle = 96 + (int)lroundf(pose.x);
                if(direction < 0) ankle = 192 - ankle;
                int ox = ankle - px, oy = 138 - (int)lroundf(pose.lift);
                float angle = (direction < 0 ? -pose.angle : pose.angle) * 3.14159265f / 1800.0f;
                for(unsigned stage=0;stage<3;stage++) {
                const lv_image_dsc_t *body=side_bodies[direction<0][stage];
                unsigned overlap = 0;
                for(int y = 0; y < image->header.h; y++) {
                    for(int x = 0; x < image->header.w; x++) {
                        if(alpha_at(image, x, y) < 180) continue;
                        int bx = ox + px + (int)lroundf((x-px) * cosf(angle) - (y-py) * sinf(angle));
                        int by = oy + py + (int)lroundf((x-px) * sinf(angle) + (y-py) * cosf(angle));
                        float tilt=direction*WALK_BODY_TILT*sinf(6.2831853f*distance/WALK_STRIDE_PX)*3.14159265f/1800.0f;
                        float dx=bx-96,dy=by-(int)lroundf(walk_body_y(distance))-154;
                        int sx=96+(int)lroundf(dx*cosf(tilt)+dy*sinf(tilt));
                        int sy=154+(int)lroundf(-dx*sinf(tilt)+dy*cosf(tilt));
                        if(alpha_at(body, sx, sy) >= 180) overlap++;
                    }
                }
                if(overlap < minimum) minimum = overlap;
                }
            }
        }
    }
    fprintf(stderr, "Attachment: minimum %u opaque overlap pixels across 4,320 foot/body poses.\n", minimum);
    assert(minimum >= 8);
}

static void verify_gait(void)
{
    tuft_motion_t hair={0};
    tuft_motion_step(&hair,0,0,0); tuft_motion_step(&hair,20,20,0);
    assert(hair.bend<0 && fabsf(hair.bend)<=2.4f);
    bool rebounded=false;
    for(unsigned time=40;time<=2000;time+=20) {
        tuft_motion_step(&hair,time,20,0);
        assert(fabsf(hair.bend)<=2.4f);
        if(hair.bend>0.1f) rebounded=true;
    }
    assert(rebounded && hair.bend==0 && hair.velocity==0);
    const lv_image_dsc_t *raw=frames[0][0];
    const lv_image_dsc_t *bent=tuft_apply(raw,2.4f);
    unsigned root=tuft_base_y(raw);
    assert(root>24 && memcmp(raw->data,bent->data,root*raw->header.stride)!=0);
    assert(memcmp(raw->data+root*raw->header.stride,bent->data+root*bent->header.stride,raw->data_size-root*raw->header.stride)==0);
    assert(tuft_apply(bent,0)==raw);
    puts("PASS: tuft lags, rebounds and settles; original root/head/face/body pixels below root unchanged.");
    walk_foot_pose_t previous[2] = {walk_foot_pose(0, 0), walk_foot_pose(0, 1)};
    float previous_world[2] = {previous[0].x, previous[1].x};
    unsigned support[2] = {0}, swing[2] = {0};
    for(unsigned sample = 1; sample <= 10000; sample++) {
        float distance = sample * (10.0f*WALK_STRIDE_PX/10000);
        assert(fabsf(walk_support_x(distance)-walk_support_x(distance-0.001f))<0.1f);
        walk_foot_pose_t poses[2] = {walk_foot_pose(distance, 0), walk_foot_pose(distance, 1)};
        assert(poses[0].planted || poses[1].planted);
        for(unsigned foot = 0; foot < 2; foot++) {
            walk_foot_pose_t pose = poses[foot];
            assert(pose.lift >= 0 && pose.lift <= WALK_SWING_LIFT_PX + 0.001f);
            float world = distance + pose.x;
            if(pose.planted) {
                support[foot]++;
                assert(pose.lift == 0 && pose.angle == 0);
                if(previous[foot].planted && pose.phase > previous[foot].phase)
                    assert(fabsf(world - previous_world[foot]) < 0.001f);
            }
            else swing[foot]++;
            previous[foot] = pose;
            previous_world[foot] = world;
        }
    }
    for(unsigned foot = 0; foot < 2; foot++) {
        assert(support[foot] > 5450 && support[foot] < 5750);
        assert(swing[foot] > 4250 && swing[foot] < 4550);
    }
    float release=WALK_STRIDE_PX*WALK_STANCE_FRACTION;
    assert(fabsf(walk_foot_pose(release - 0.0001f, 0).x -
                 walk_foot_pose(release + 0.0001f, 0).x) < 0.001f);
    assert(fabsf(walk_foot_pose(WALK_STRIDE_PX - 0.0001f, 0).x -
                 walk_foot_pose(WALK_STRIDE_PX + 0.0001f, 0).x) < 0.001f);
    assert(walk_travel(0, 2400, 60) == 0);
    assert(walk_travel(2400, 2400, 60) == 60);
    puts("PASS: 10,000 gait samples, stable planted feet, short 56/44 chick steps, no simultaneous airborne feet.");
}

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    (void)area;
    memcpy(screen_buffer, pixels, sizeof(screen_buffer));
    lv_display_flush_ready(display);
}

static void save_preview(lv_display_t *display, const char *path)
{
    lv_refr_now(display);
    FILE *file = fopen(path, "wb");
    assert(file);
    fprintf(file, "P6\n410 502\n255\n");
    for(unsigned i = 0; i < 410 * 502; i++) {
        const uint8_t *pixel = screen_buffer + i * 4;
        uint8_t rgb[] = {pixel[2], pixel[1], pixel[0]};
        fwrite(rgb, 1, 3, file);
    }
    fclose(file);
}

int main(void)
{
    pet_scene.width=410;
    assert(scene_cloud_x(0,0)==scene_cloud_x(PET_CLOUD_PERIOD_MS,0));
    uint32_t cloud_wrap=PET_CLOUD_PERIOD_MS-cloud_phase_offset[0];
    assert(scene_cloud_x(cloud_wrap-1,0)>410);
    assert(scene_cloud_x(cloud_wrap,0)==-160);
    verify_cloud_art();
    assert(scene_wind_wave(0,1000)==0 && scene_wind_wave(5000,1000)==0);
    assert(fabsf(scene_wind_wave(2250,1000))<fabsf(scene_wind_wave(250,1000)));
    double means[2][3]={{0}};
    const lv_image_dsc_t *materials[2]={frames[0][3],side_bodies[0][0]};
    for(unsigned n=0;n<2;n++) {
        const lv_image_dsc_t *raw=materials[n], *graded=pet_color_image(raw);
        if(n==0) assert(raw==graded); /* Front artwork is byte-for-byte original. */
        unsigned count=0;
        for(unsigned i=0;i<raw->data_size;i+=4) {
            assert(raw->data[i+3]==graded->data[i+3]);
            if(raw->data[i+2]<90 && raw->data[i+1]<80)
                assert(memcmp(raw->data+i,graded->data+i,4)==0);
            const uint8_t *p=graded->data+i;
            if(yellow_material(raw->data[i+2],raw->data[i+1],raw->data[i]))
                assert(memcmp(raw->data+i,graded->data+i,4)==0);
            if(p[3]>200 && p[2]>235 && p[1]>190 && p[0]<130) {
                for(unsigned c=0;c<3;c++) means[n][c]+=p[2-c];
                count++;
            }
        }
        for(unsigned c=0;c<3;c++) means[n][c]/=count;
    }
    /* Side is the material authority: reject the old amber front palette.
     * Allow soft shading, but not a different hue or visibly pale saturation. */
    for(unsigned c=0;c<3;c++) assert(fabs(means[0][c]-means[1][c])<7.0);
    double hue[2],saturation[2];
    for(unsigned n=0;n<2;n++) {
        hue[n]=60*(means[n][1]-means[n][2])/(means[n][0]-means[n][2]);
        saturation[n]=(means[n][0]-means[n][2])/means[n][0];
    }
    assert(fabs(hue[0]-hue[1])<0.8);
    assert(fabs(saturation[0]-saturation[1])<0.035);
    printf("PASS: unified ungraded artwork; body means front %.1f/%.1f/%.1f, side %.1f/%.1f/%.1f; configured cloud period and damped wind.\n",means[0][0],means[0][1],means[0][2],means[1][0],means[1][1],means[1][2]);
    verify_gait();
    verify_attachment();
    lv_init();
    lv_display_t *display = lv_display_create(410, 502);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flush);
    pet_create(lv_screen_active(), 410, 502);
    show_frame(3,1);
    save_preview(display,"build/animation-preview/palette-front.ppm");
    show_walk(0,1);
    save_preview(display,"build/animation-preview/palette-side.ppm");
    enter_phase(REST);
    lv_timer_pause(s_pet.timer);
    s_pet.mouth_started=lv_tick_get(); s_pet.mouth_wait=18000;
    pet_scene.gust=true; pet_scene.wind_started=lv_tick_get();
    pet_scene.direction=1; pet_scene.strength=1.0f;
    for(unsigned frame=0;frame<70;frame++) {
        for(unsigned tick=0;tick<5;tick++) { lv_tick_inc(20); lv_timer_handler(); }
        for(unsigned layer=0;layer<2;layer++) for(unsigned half=0;half<2;half++) {
            const lv_image_dsc_t *source=pet_scene.plants_source[layer][half];
            assert(memcmp(source->data+(source->header.h-1)*source->header.stride,
                pet_scene.plants_pixels[layer][half]+(source->header.h-1)*source->header.stride,source->header.stride)==0);
        }
        char path[120]; snprintf(path,sizeof(path),"build/animation-preview/atmosphere-%02u.ppm",frame);
        save_preview(display,path);
    }
    lv_timer_resume(s_pet.timer);
    enter_phase(REST);
    save_preview(display, "build/animation-preview/rest-open.ppm");
    s_pet.blink_stage = 2;
    render_eyes();
    save_preview(display, "build/animation-preview/rest-closed.ppm");
    s_pet.blink_stage = 0;

    enter_phase(REST);
    s_pet.duration=30000;
    s_pet.head_wait=4000;
    s_pet.sky_chosen=false;
    s_pet.ground_chosen=false;
    rest_head_tick(4000);
    assert(s_pet.head_turning && s_pet.head_frame==0);
    rest_head_tick(4100);
    assert(abs(s_pet.gaze_offset)==2 && s_pet.head_frame==0);
    s_pet.blink_stage=0; render_eyes();
    save_preview(display,"build/animation-preview/gaze-lead.ppm");
    lv_area_t face_origin;
    lv_obj_get_coords(s_pet.img,&face_origin);
    for(unsigned eye=0;eye<2;eye++) {
        const eye_region_t *region=&daily_eyes[0].eyes[eye];
        unsigned count=0;
        double mean=0,raw_mean=0;
        unsigned raw_count=0;
        for(int y=region->y;y<region->y+region->h;y++) for(int x=region->x-3;x<region->x+region->w+3;x++) {
            const uint8_t *p=screen_buffer+((face_origin.y1+y)*410+face_origin.x1+x)*4;
            if(p[2]<85 && p[1]<75 && p[0]<65) { mean+=x; count++; }
            const uint8_t *raw=frames[0][0]->data+y*frames[0][0]->header.stride+x*4;
            if(raw[2]<85 && raw[1]<75 && raw[0]<65) { raw_mean+=x; raw_count++; }
        }
        assert(count>50 && raw_count>50);
        double shift=mean/count-raw_mean/raw_count;
        assert(fabs(shift-s_pet.gaze_offset)<0.6);
    }
    rest_head_tick(4440);
    assert(s_pet.head_frame==3 && s_pet.mouth==NULL);
    rest_head_tick(4000+880+s_pet.head_hold);
    assert(!s_pet.head_turning && s_pet.head_frame==0);
    assert(!mouth_can_call());
    lv_tick_inc(1200);
    assert(mouth_can_call());
    for(int direction=-1;direction<=1;direction+=2) for(unsigned frame=0;frame<4;frame++) {
        show_rest_head(frame,direction);
        char path[120];
        snprintf(path,sizeof(path),"build/animation-preview/head-%s-%u.ppm",direction<0 ? "left" : "right",frame);
        save_preview(display,path);
        if(frame>0) {
            const lv_image_dsc_t *image=lv_img_get_src(s_pet.img);
            for(unsigned y=150;y<192;y++)
                assert(memcmp(image->data+y*image->header.stride,frames[0][0]->data+y*frames[0][0]->header.stride,192*4)==0);
        }
    }
    show_frame(0,1);
    puts("PASS: idle head turn enters, holds and returns; original lower body/feet unchanged in both directions.");

    /* Capture the actual idle timers, including head return and independent blink. */
    enter_phase(REST);
    s_pet.duration=30000;
    s_pet.head_wait=600;
    for(unsigned frame=0;frame<100;frame++) {
        for(unsigned tick=0;tick<2;tick++) { lv_tick_inc(20); lv_timer_handler(); }
        char path[120];
        snprintf(path,sizeof(path),"build/animation-preview/idle-clip-%02u.ppm",frame);
        save_preview(display,path);
    }
    show_frame(0,1);

    enter_phase(REST);
    s_pet.duration=30000; s_pet.head_wait=4000;
    s_pet.sky_chosen=true;
    s_pet.ground_chosen=false;
    rest_head_tick(4000); rest_head_tick(4100);
    assert(s_pet.head_is_sky && s_pet.sky_used && s_pet.gaze_y==-2 && s_pet.gaze_offset==0);
    assert(!mouth_can_call() && s_pet.head_frame==0);
    rest_head_tick(4440);
    assert(s_pet.head_frame==3 && s_pet.mouth==NULL);
    for(unsigned pose=1;pose<=3;pose++) {
        show_sky_head(pose);
        const lv_image_dsc_t *image=lv_img_get_src(s_pet.img);
        assert(lv_obj_get_x(s_pet.img)==0 && lv_obj_get_y(s_pet.img)==0);
        for(unsigned y=150;y<192;y++)
            assert(memcmp(image->data+y*image->header.stride,frames[0][0]->data+y*frames[0][0]->header.stride,192*4)==0);
    }
    rest_head_tick(4000+880+s_pet.head_hold);
    assert(!s_pet.head_turning && s_pet.gaze_y==0);
    lv_tick_inc(1200); rest_head_tick(s_pet.head_wait);
    assert(s_pet.head_turning && !s_pet.head_is_sky); /* At most once this rest. */
    enter_phase(REST); s_pet.duration=30000; s_pet.head_wait=600; s_pet.sky_chosen=true; s_pet.ground_chosen=false;
    for(unsigned frame=0;frame<110;frame++) {
        for(unsigned tick=0;tick<2;tick++) { lv_tick_inc(20); lv_timer_handler(); }
        char path[120]; snprintf(path,sizeof(path),"build/animation-preview/sky-clip-%03u.ppm",frame);
        save_preview(display,path);
        assert(s_pet.x==0 && lv_obj_get_y(s_pet.img)==0);
    }
    assert(s_pet.sky_used && !s_pet.head_turning);
    s_pet.head_is_sky=false; show_frame(0,1);
    puts("PASS: sky glance leads head; fixed original lower body, closed beak, one sky look maximum per rest.");

    enter_phase(REST); s_pet.duration=30000; s_pet.head_wait=4000;
    s_pet.sky_chosen=false; s_pet.ground_chosen=true;
    rest_head_tick(4000); rest_head_tick(4100);
    assert(s_pet.head_is_ground && !s_pet.head_is_sky && s_pet.ground_used);
    assert(s_pet.gaze_y==2 && s_pet.gaze_offset==0 && !mouth_can_call());
    rest_head_tick(4440);
    assert(s_pet.head_frame==3 && s_pet.mouth==NULL);
    for(unsigned pose=1;pose<=3;pose++) {
        show_ground_head(pose);
        const lv_image_dsc_t *image=lv_img_get_src(s_pet.img);
        for(unsigned y=150;y<192;y++)
            assert(memcmp(image->data+y*image->header.stride,frames[0][0]->data+y*frames[0][0]->header.stride,192*4)==0);
    }
    rest_head_tick(4000+880+s_pet.head_hold);
    assert(!s_pet.head_turning && s_pet.gaze_y==0);
    lv_tick_inc(1200); rest_head_tick(s_pet.head_wait);
    assert(s_pet.head_turning && !s_pet.head_is_ground && !s_pet.head_is_sky);
    enter_phase(REST); s_pet.duration=30000; s_pet.head_wait=600;
    s_pet.sky_chosen=false; s_pet.ground_chosen=true;
    for(unsigned frame=0;frame<110;frame++) {
        for(unsigned tick=0;tick<2;tick++) { lv_tick_inc(20); lv_timer_handler(); }
        char path[120]; snprintf(path,sizeof(path),"build/animation-preview/ground-clip-%03u.ppm",frame);
        save_preview(display,path);
        assert(s_pet.x==0 && lv_obj_get_y(s_pet.img)==0);
    }
    assert(s_pet.ground_used && !s_pet.head_turning);
    s_pet.head_is_ground=false; show_frame(0,1);
    puts("PASS: ground glance leads small head dip; fixed lower body, one downward observation per rest.");

    for(unsigned view=0;view<2;view++) {
        enter_phase(view ? STAND : REST);
        s_pet.duration=30000; s_pet.head_wait=100000;
        s_pet.front_flap_enabled=true; s_pet.front_flap_wait=600;
        s_pet.calling=false; s_pet.head_has_returned=false;
        s_pet.mouth_started=lv_tick_get(); s_pet.mouth_wait=40000;
        for(unsigned frame=0;frame<70;frame++) {
            for(unsigned tick=0;tick<2;tick++) { lv_tick_inc(20); lv_timer_handler(); }
            if(s_pet.front_flapping) {
                const lv_image_dsc_t *body=lv_img_get_src(s_pet.img);
                assert(!s_pet.head_turning && !mouth_can_call());
                assert(lv_obj_get_x(s_pet.img)==0 && lv_obj_get_y(s_pet.img)==0);
                for(unsigned y=tuft_base_y(s_pet.front_base);y<192;y++)
                    assert(memcmp(body->data+y*body->header.stride+52*4,s_pet.front_base->data+y*s_pet.front_base->header.stride+52*4,88*4)==0);
                for(unsigned y=150;y<192;y++) for(unsigned x=44;x<148;x++) {
                    const uint8_t *original=s_pet.front_base->data+y*s_pet.front_base->header.stride+x*4;
                    assert(memcmp(original,body->data+y*body->header.stride+x*4,4)==0);
                }
            }
            char path[120]; snprintf(path,sizeof(path),"build/animation-preview/front-flap-%u-%02u.ppm",view,frame);
            save_preview(display,path);
        }
        assert(s_pet.front_flap_used && !s_pet.front_flapping);
        /* The current idle tuft drive may wrap the returned base in a warp. */
        const lv_image_dsc_t *folded=lv_img_get_src(s_pet.img);
        unsigned base_y=tuft_base_y(s_pet.front_base);
        assert(memcmp(folded->data+base_y*folded->header.stride,
                      s_pet.front_base->data+base_y*s_pet.front_base->header.stride,
                      folded->data_size-base_y*folded->header.stride)==0);
    }
    puts("PASS: front seated/standing flap preserves centre/feet, fixed roots, one burst, returns to original art.");

    enter_phase(REST); s_pet.duration=1200; s_pet.head_wait=100000;
    s_pet.will_walk=false; s_pet.front_flap_enabled=false;
    memset(&s_pet.tuft,0,sizeof(s_pet.tuft));
    for(unsigned frame=0;frame<100;frame++) {
        for(unsigned tick=0;tick<4;tick++) {
            lv_tick_inc(20); lv_timer_handler();
            if(s_pet.phase==STAND) { s_pet.duration=3000; s_pet.will_walk=false; }
            s_pet.front_flap_enabled=false; s_pet.head_wait=100000;
        }
        char path[120]; snprintf(path,sizeof(path),"build/animation-preview/tuft-clip-%02u.ppm",frame);
        save_preview(display,path);
        assert(fabsf(s_pet.tuft.bend)<=2.4f && s_pet.x==0);
    }

    for(unsigned view = 0; view < 4; view++) {
        if(view >= 2) show_walk(0, view == 2 ? 1 : -1);
        else show_frame(view == 0 ? 3 : 4, 1);
        assert(s_pet.mouth != NULL && s_pet.mouth->count > 0);
        for(unsigned stage = 0; stage < 3; stage++) {
            s_pet.mouth_stage = stage;
            refresh_side_portrait();
            lv_obj_invalidate(s_pet.img);
            char path[120];
            snprintf(path, sizeof(path), "build/animation-preview/mouth-%u-%u.ppm", view, stage);
            save_preview(display, path);
            if(s_pet.mouth->mode == 2)
                assert(lv_obj_get_style_bitmap_mask_src(s_pet.img,0)==NULL);
            if(stage == 0) memcpy(mouth_baseline, screen_buffer, sizeof(mouth_baseline));
            else if(view<2) {
                lv_area_t origin;
                lv_obj_get_coords(s_pet.img, &origin);
                const mouth_pose_t *m = s_pet.mouth;
                int mouth_x = s_pet.eye_direction < 0 ? 192 - m->x - m->w : m->x;
                unsigned changed = 0;
                for(unsigned pixel = 0; pixel < 410 * 502; pixel++) {
                    if(memcmp(mouth_baseline + pixel*4, screen_buffer + pixel*4, 4) == 0) continue;
                    int x = pixel % 410, y = pixel / 410;
                    assert(x >= origin.x1+mouth_x && x < origin.x1+mouth_x+m->w);
                    assert(y >= origin.y1+m->y && y < origin.y1+m->y+m->h);
                    changed++;
                }
                assert(changed > 5);
            }
        }
    }
    /* Inspect the actual complete side frames while holding the walk pose. */
    lv_timer_pause(s_pet.timer);
    s_pet.phase=SETTLE_OUT;
    s_pet.mouth_stage=0;
    s_pet.calling=false;
    show_walk(0,1);
    s_pet.mouth_started=lv_tick_get();
    s_pet.mouth_wait=320;
    s_pet.blink_started=lv_tick_get();
    s_pet.blink_wait=480;
    for(unsigned frame=0;frame<30;frame++) {
        for(unsigned tick=0;tick<2;tick++) { lv_tick_inc(20); lv_timer_handler(); }
        char path[120];
        snprintf(path,sizeof(path),"build/animation-preview/side-mouth-clip-%02u.ppm",frame);
        save_preview(display,path);
    }
    assert(!s_pet.calling && s_pet.mouth_stage==0);
    lv_timer_resume(s_pet.timer);
    enter_phase(REST);
    s_pet.mouth_stage = 0;
    s_pet.calling = false;
    s_pet.mouth_started = lv_tick_get();
    s_pet.mouth_wait = 320;
    s_pet.blink_started = lv_tick_get();
    s_pet.blink_wait = 480;
    for(unsigned frame = 0; frame < 30; frame++) {
        for(unsigned tick = 0; tick < 2; tick++) { lv_tick_inc(20); lv_timer_handler(); }
        char path[120];
        snprintf(path, sizeof(path), "build/animation-preview/mouth-clip-%02u.ppm", frame);
        save_preview(display, path);
    }
    assert(!s_pet.calling && s_pet.mouth_stage == 0);
    s_pet.blink_stage = 0;

    show_walk(0, 1);
    assert(lv_img_get_src(s_pet.img) == &walk_expression_image);
    for(unsigned frame = 0; frame < 12; frame++) {
        char path[120];
        show_walk(frame * (WALK_STRIDE_PX / 12.0f), 1);
        snprintf(path, sizeof(path), "build/animation-preview/gait-%02u.ppm", frame);
        save_preview(display, path);
        assert(lv_obj_get_y(s_pet.img)>=-WALK_BODY_RISE_PX && lv_obj_get_y(s_pet.img)<=0);
        const lv_image_dsc_t *animated=lv_img_get_src(s_pet.img);
        assert(lv_obj_get_parent(s_pet.wing)==s_pet.img);
        assert(!lv_obj_is_hidden(s_pet.wing));
        /* Rump/body texture stays identical; only the separate wing rotates. */
        if(frame==0) memcpy(stable_body,animated->data,animated->data_size);
        else assert(memcmp(stable_body,animated->data,animated->data_size)==0);
        const lv_image_dsc_t *original=pet_color_image(side_bodies[0][0]);
        /* Local wing motion does not repaint or distort the face. */
        for(unsigned y=0;y<88;y++)
            assert(memcmp(animated->data+y*animated->header.stride,
                          original->data+y*original->header.stride,192*4)==0);
    }
    show_walk(WALK_STRIDE_PX*0.25f, 1);
    assert(s_pet.foot_poses[0].planted && !s_pet.foot_poses[1].planted);
    save_preview(display, "build/animation-preview/walk-right.ppm");
    show_walk(WALK_STRIDE_PX*0.75f, 1);
    assert(!s_pet.foot_poses[0].planted && s_pet.foot_poses[1].planted);
    show_walk(7.5f, -1);
    s_pet.blink_stage = 2;
    render_eyes();
    save_preview(display, "build/animation-preview/walk-left-closed.ppm");

    show_walk(60,1);
    enter_phase(SETTLE_OUT);
    assert(!mouth_can_call());
    lv_tick_inc(90); routine_timer_cb(NULL);
    assert(s_pet.wing_frame==5 && s_pet.wing_settle_start==3);
    lv_tick_inc(90); routine_timer_cb(NULL);
    assert(s_pet.wing_frame==0);
    puts("PASS: continuous support shadow; raised wing eases to rest before turning; no settling chirp.");

    /* Verify real drawn frames, fixed shoulder attachment and mirrored views. */
    for(unsigned direction=0;direction<2;direction++) for(unsigned frame=0;frame<6;frame++) {
        const lv_image_dsc_t *wing=wing_animation[direction][frame];
        unsigned area=0,base_area=0;
        for(unsigned pixel=0;pixel<wing->data_size;pixel+=4) {
            area+=wing->data[pixel+3]>160;
            base_area+=wing_animation[direction][0]->data[pixel+3]>160;
        }
        assert(area>base_area*0.85f && area<base_area*1.15f);
        assert(alpha_at(wing,direction ? 9 : 54,34)>160);
        assert(alpha_at(side_bodies[direction][0],direction ? 118 : 73,108)>180);
        if(frame) assert(memcmp(wing->data,wing_animation[direction][frame-1]->data,wing->data_size)!=0);
        show_walk(0,direction ? -1 : 1);
        show_wing_frame(frame,direction ? -1 : 1);
        char path[120]; snprintf(path,sizeof(path),"build/animation-preview/wing-pose-%u-%u.ppm",direction,frame);
        save_preview(display,path);
    }
    s_pet.blink_stage=0;
    for(unsigned frame=0;frame<30;frame++) {
        show_walk(frame*WALK_STRIDE_PX/30.0f,1);
        char path[120]; snprintf(path,sizeof(path),"build/animation-preview/wing-flap-%02u.ppm",frame);
        save_preview(display,path);
    }
    puts("PASS: six distinct drawn wing poses, mirrored shoulder anchoring, rest frame at pause/settle.");

    /* Both daily choices are checked directly; public WALK still forces a walk. */
    s_pet.x=0;
    lv_obj_set_style_translate_x(s_pet.pet,0,0);
    enter_phase(STAND);
    s_pet.will_walk=false;
    lv_tick_inc(s_pet.duration); routine_timer_cb(NULL);
    assert(s_pet.phase==SIT && s_pet.x==0);
    lv_tick_inc(s_pet.duration); routine_timer_cb(NULL);
    assert(s_pet.phase==REST && s_pet.duration>=12000 && s_pet.duration<=20000);
    enter_phase(STAND);
    s_pet.will_walk=true;
    lv_tick_inc(s_pet.duration); routine_timer_cb(NULL);
    assert(s_pet.phase==LOOK);
    pet_set_state(PET_STATE_WALK);
    assert(s_pet.phase==LOOK);
    assert(s_pet.mouth_wait>=10000 && s_pet.mouth_wait<=20000);
    puts("PASS: stand/sit and walk branches; head-return quiet beat; current 10–20s beak interval.");

    s_pet.x=0;
    s_pet.will_walk=false;
    enter_phase(RISE);
    for(unsigned frame=0;frame<75;frame++) {
        for(unsigned tick=0;tick<4;tick++) {
            lv_tick_inc(20); lv_timer_handler();
            if(s_pet.phase==STAND) s_pet.duration=3000; /* Valid short observation. */
        }
        char path[120];
        snprintf(path,sizeof(path),"build/animation-preview/quiet-branch-%02u.ppm",frame);
        save_preview(display,path);
        assert(s_pet.x==0 && s_pet.phase!=OUT && s_pet.phase!=BACK);
    }

    /* Short actual out/turn/back clip, with the same timers as the simulator. */
    s_pet.blink_stage = 0;
    s_pet.direction = 1;
    s_pet.excursion = 60;
    enter_phase(OUT);
    /* Forced valid pause for inspection: the production choice remains 25%. */
    s_pet.walk_pause=true;
    s_pet.walk_pause_ms=800;
    s_pet.duration=60*WALK_CYCLE_MS/WALK_STRIDE_PX+800;
    unsigned pause_frames=0;
    for(unsigned frame = 0; frame < 90; frame++) {
        for(unsigned tick = 0; tick < 4; tick++) { lv_tick_inc(20); lv_timer_handler(); }
        char path[120];
        snprintf(path, sizeof(path), "build/animation-preview/walk-clip-%02u.ppm", frame);
        save_preview(display, path);
        uint32_t travel_elapsed=lv_tick_elaps(s_pet.started);
        if(s_pet.phase==OUT && travel_elapsed>=s_pet.walk_first_ms && travel_elapsed<s_pet.walk_first_ms+s_pet.walk_pause_ms) {
            pause_frames++;
            assert(s_pet.x==24);
            assert(s_pet.foot_poses[0].planted && s_pet.foot_poses[1].planted);
            assert(s_pet.wing_frame==0 && s_pet.mouth_stage==0);
            assert(lv_obj_get_y(s_pet.img)==0);
        }
    }
    assert(pause_frames>=9);
    puts("PASS: 2px original eye layers lead head yaw; mid-walk pause has two grounded feet, resting wing and no drift.");

    pet_set_state(PET_STATE_IDLE);
    unsigned transitions = 0, blinks = 0, returns = 0, walk_blinks = 0, calls = 0;
    daily_phase_t previous = s_pet.phase;
    unsigned previous_blink = s_pet.blink_stage;
    unsigned previous_mouth = s_pet.mouth_stage;
    for(unsigned time = 0; time < 300000; time += 20) {
        lv_tick_inc(20);
        lv_timer_handler();
        if(previous != s_pet.phase) {
            transitions++;
            if(s_pet.phase == FACE) { assert(s_pet.x == 0); returns++; }
            previous = s_pet.phase;
        }
        if(previous_blink != 2 && s_pet.blink_stage == 2) {
            blinks++;
            if(s_pet.phase == OUT || s_pet.phase == BACK) walk_blinks++;
        }
        previous_blink = s_pet.blink_stage;
        if(previous_mouth != 2 && s_pet.mouth_stage == 2) calls++;
        previous_mouth = s_pet.mouth_stage;
        if(s_pet.phase == OUT || s_pet.phase == BACK)
            assert(s_pet.mouth_stage == 0 && !s_pet.calling);
        assert(lv_obj_get_width(s_pet.pet) == 192);
        assert(lv_obj_get_height(s_pet.pet) == 192);
        assert(lv_obj_get_style_transform_scale_x(s_pet.pet, 0) == 256);
        assert(lv_obj_get_style_transform_scale_y(s_pet.pet, 0) == 256);
    }
    assert(transitions >= 20 && returns >= 2 && blinks >= 15 && walk_blinks > 0 && calls >= 3);
    pet_set_state(PET_STATE_SLEEP);
    lv_tick_inc(6000);
    lv_timer_handler();
    assert(s_pet.blink_stage == 0 && !s_pet.awake);
    assert(s_pet.mouth_stage == 0 && !s_pet.calling);
    printf("PASS: plant roots anchored throughout gust; frontal mouth regions, unmasked side portraits; %u quiet calls.\n", calls);
    printf("PASS: bounded grounded torso/wing motion, original face pixels; %u phase transitions, %u center returns, %u blinks (%u while walking); sleep stops face actions.\n", transitions, returns, blinks, walk_blinks);
    lv_deinit();
    return 0;
}
