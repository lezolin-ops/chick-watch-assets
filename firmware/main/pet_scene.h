/* Independent painted clouds and passing, damped gusts. */
#ifndef PET_SCENE_H
#define PET_SCENE_H
#include "pet_scene_assets.c"
#include "pet_cloud_assets.c"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define PET_CLOUD_PERIOD_MS 480000U   /* 云 8 分钟一圈（原 1 小时） */
/* Start both clouds in view; each still travels left-to-right at the old speed. */
static const uint32_t cloud_phase_offset[2]={
    (uint32_t)((uint64_t)PET_CLOUD_PERIOD_MS*184/730),
    (uint32_t)((uint64_t)PET_CLOUD_PERIOD_MS*470/730)
};
static struct {
    lv_obj_t *clouds[2], *grass[2], *flowers[2];
    lv_timer_t *cloud_timer,*wind_timer;
    uint32_t cloud_started,wind_started,wind_wait;
    int width,direction;
    float strength;
    bool gust;
    const lv_image_dsc_t *plants_source[2][2];
    lv_image_dsc_t plants_image[2][2];
    uint8_t *plants_pixels[2][2];
    int last_bend[2][2];
} pet_scene;

static void scene_bend_plant(unsigned layer,unsigned half,int bend)
{
    if(bend==pet_scene.last_bend[layer][half]) return;
    const lv_image_dsc_t *source=pet_scene.plants_source[layer][half];
    uint8_t *pixels=pet_scene.plants_pixels[layer][half];
    if(!pixels) return;
    memset(pixels,0,source->data_size);
    for(unsigned y=0;y<source->header.h;y++) {
        float height=(source->header.h-1-y)/(float)(source->header.h-1);
        int shift=(int)lroundf(bend*height*height);
        int from=shift<0 ? -shift : 0, to=shift>0 ? shift : 0;
        int width=source->header.w-abs(shift);
        if(width>0) memcpy(pixels+y*source->header.stride+to*4,
                          source->data+y*source->header.stride+from*4,width*4);
    }
    pet_scene.last_bend[layer][half]=bend;
    lv_obj_invalidate(layer==0 ? pet_scene.grass[half] : pet_scene.flowers[half]);
}

static float scene_cloud_x(uint32_t elapsed,unsigned cloud)
{
    uint32_t phase=(elapsed+cloud_phase_offset[cloud])%PET_CLOUD_PERIOD_MS;
    return -160.0f+(float)((uint64_t)phase*(pet_scene.width+320)/PET_CLOUD_PERIOD_MS);
}
static void scene_cloud_tick(lv_timer_t *timer)
{
    (void)timer;
    uint32_t elapsed=lv_tick_elaps(pet_scene.cloud_started);
    for(unsigned i=0;i<2;i++) lv_obj_set_x(pet_scene.clouds[i],(lv_coord_t)lrintf(scene_cloud_x(elapsed,i)));
}
static float scene_wind_wave(float t,float period)
{
    if(t<0 || t>4500) return 0;
    float fade=expf(-t/1400.0f)*fminf(t/350.0f,1.0f);
    return sinf(t*6.2831853f/period)*fade;
}
static void scene_wind_tick(lv_timer_t *timer)
{
    (void)timer;
    uint32_t elapsed=lv_tick_elaps(pet_scene.wind_started);
    if(!pet_scene.gust && elapsed>=pet_scene.wind_wait) {
        pet_scene.gust=true; pet_scene.wind_started=lv_tick_get(); elapsed=0;
        pet_scene.direction=lv_rand(0,1) ? 1 : -1;
        pet_scene.strength=lv_rand(70,115)/100.0f;
    }
    for(unsigned half=0;half<2;half++) {
        float t=pet_scene.gust ? (float)elapsed-((pet_scene.direction>0 ? half : 1-half)*480) : -1;
        int grass=(int)lroundf(18*pet_scene.strength*pet_scene.direction*scene_wind_wave(t,1000));
        int flowers=(int)lroundf(16*pet_scene.strength*pet_scene.direction*scene_wind_wave(t-120,1250));
        scene_bend_plant(0,half,grass);
        scene_bend_plant(1,half,flowers);
    }
    if(pet_scene.gust && elapsed>5100) {
        pet_scene.gust=false; pet_scene.wind_started=lv_tick_get(); pet_scene.wind_wait=lv_rand(7000,18000);
    }
}
static void scene_create_background(lv_obj_t *parent,int w,int h)
{
    pet_scene.width=w;
    lv_obj_t *base=lv_img_create(parent); lv_img_set_src(base,&pet_scene_base);
    lv_obj_set_pos(base,0,0); lv_obj_set_size(base,w,h); lv_obj_clear_flag(base, LV_OBJ_FLAG_CLICKABLE);
    const lv_image_dsc_t *cloud_art[2]={&pet_scene_cloud_near,&pet_scene_cloud_far};
    for(unsigned i=0;i<2;i++) {
        lv_obj_t *group=lv_img_create(parent);
        lv_img_set_src(group,cloud_art[i]);
        lv_obj_set_size(group,cloud_art[i]->header.w,cloud_art[i]->header.h);
        lv_obj_set_pos(group,0,i ? 88 : 34);
        lv_obj_set_style_opa_layered(group,i ? 205 : 245,0);
        lv_obj_clear_flag(group, LV_OBJ_FLAG_CLICKABLE); lv_obj_clear_flag(group, LV_OBJ_FLAG_SCROLLABLE);
        pet_scene.clouds[i]=group;
    }
    pet_scene.cloud_started=lv_tick_get(); scene_cloud_tick(NULL);
    pet_scene.cloud_timer=lv_timer_create(scene_cloud_tick,33,NULL);
}
static void scene_create_foreground(lv_obj_t *parent)
{
    const lv_image_dsc_t *sources[2][2]={{&pet_scene_grass_0,&pet_scene_grass_1},{&pet_scene_flowers_0,&pet_scene_flowers_1}};
    for(unsigned layer=0;layer<2;layer++) for(unsigned half=0;half<2;half++) {
        pet_scene.plants_source[layer][half]=sources[layer][half];
        pet_scene.plants_image[layer][half]=*sources[layer][half];
        pet_scene.plants_pixels[layer][half]=malloc(sources[layer][half]->data_size);
        if(pet_scene.plants_pixels[layer][half]) {
            memcpy(pet_scene.plants_pixels[layer][half],sources[layer][half]->data,sources[layer][half]->data_size);
            pet_scene.plants_image[layer][half].data=pet_scene.plants_pixels[layer][half];
            pet_scene.plants_image[layer][half].header.flags|=LV_IMAGE_FLAGS_MODIFIABLE;
        }
        lv_obj_t *image=lv_img_create(parent); lv_img_set_src(image,&pet_scene.plants_image[layer][half]);
        lv_obj_set_pos(image,half*205,340); lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
        if(layer==0) pet_scene.grass[half]=image; else pet_scene.flowers[half]=image;
    }
    pet_scene.wind_started=lv_tick_get(); pet_scene.wind_wait=lv_rand(7000,18000);
    pet_scene.wind_timer=lv_timer_create(scene_wind_tick,40,NULL);
}
#endif
