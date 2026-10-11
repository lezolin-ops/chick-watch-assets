/* Independent atmospheric layers: one-hour clouds and passing, damped gusts. */
#ifndef PET_SCENE_H
#define PET_SCENE_H
#include "pet_scene_assets.c"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define PET_CLOUD_PERIOD_MS 3600000U
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

static int scene_cloud_x(uint32_t elapsed,unsigned cloud)
{
    uint32_t phase=(elapsed+(cloud ? 2250000U : 900000U))%PET_CLOUD_PERIOD_MS;
    return -160+(int)((uint64_t)phase*(pet_scene.width+320)/PET_CLOUD_PERIOD_MS);
}
static void scene_cloud_tick(lv_timer_t *timer)
{
    (void)timer;
    uint32_t elapsed=lv_tick_elaps(pet_scene.cloud_started);
    for(unsigned i=0;i<2;i++) lv_obj_set_x(pet_scene.clouds[i],scene_cloud_x(elapsed,i));
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
static void scene_cloud_part(lv_obj_t *parent,int x,int y,int w,int h,bool pale)
{
    lv_obj_t *part=lv_obj_create(parent); lv_obj_remove_style_all(part);
    lv_obj_set_pos(part,x,y); lv_obj_set_size(part,w,h);
    lv_obj_set_style_radius(part,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_bg_color(part,lv_color_hex(pale ? 0xEFF8FE : 0xFFFFFF),0);
    lv_obj_set_style_bg_opa(part,LV_OPA_COVER,0);
    lv_obj_set_clickable(part,false); lv_obj_set_scrollable(part,false);
}
static void scene_create_background(lv_obj_t *parent,int w,int h)
{
    pet_scene.width=w;
    lv_obj_t *base=lv_img_create(parent); lv_img_set_src(base,&pet_scene_base);
    lv_obj_set_pos(base,0,0); lv_obj_set_size(base,w,h); lv_obj_set_clickable(base,false);
    for(unsigned i=0;i<2;i++) {
        lv_obj_t *group=lv_obj_create(parent); lv_obj_remove_style_all(group);
        lv_obj_set_size(group,i ? 98 : 148,i ? 42 : 66);
        lv_obj_set_pos(group,0,i ? 96 : 25);
        lv_obj_set_style_opa_layered(group,i ? 178 : 245,0);
        lv_obj_set_clickable(group,false); lv_obj_set_scrollable(group,false);
        if(i==0) {
            /* Nearby cumulus: tall, uneven lobes and a rounded full base. */
            scene_cloud_part(group,8,39,132,23,true);
            scene_cloud_part(group,18,25,55,34,false);
            scene_cloud_part(group,48,5,60,53,false);
            scene_cloud_part(group,89,23,44,36,false);
            scene_cloud_part(group,6,40,136,17,false);
        }
        else {
            /* Distant cloud: flatter, smaller, asymmetrical and more diffuse. */
            scene_cloud_part(group,4,26,91,12,true);
            scene_cloud_part(group,9,17,42,19,false);
            scene_cloud_part(group,40,9,33,25,false);
            scene_cloud_part(group,66,22,29,14,false);
        }
        pet_scene.clouds[i]=group;
    }
    pet_scene.cloud_started=lv_tick_get(); scene_cloud_tick(NULL);
    pet_scene.cloud_timer=lv_timer_create(scene_cloud_tick,1000,NULL);
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
        lv_obj_set_pos(image,half*205,340); lv_obj_set_clickable(image,false);
        if(layer==0) pet_scene.grass[half]=image; else pet_scene.flowers[half]=image;
    }
    pet_scene.wind_started=lv_tick_get(); pet_scene.wind_wait=lv_rand(7000,18000);
    pet_scene.wind_timer=lv_timer_create(scene_wind_tick,40,NULL);
}
#endif
