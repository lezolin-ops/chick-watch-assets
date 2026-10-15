/* Standalone ThorVG preview; does not change the running daily-pet scene. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "lvgl/lvgl.h"
#include "../lvgl/src/libs/thorvg/thorvg_capi.h"
static uint32_t pixels[480*480];
int main(int argc,char **argv)
{
    assert(argc==3);
    lv_init(); /* Bundled ThorVG allocators use LVGL's memory subsystem. */
    FILE *input=fopen(argv[1],"rb");assert(input);
    fseek(input,0,SEEK_END);long size=ftell(input);rewind(input);
    char *json=malloc(size+1);assert(json);assert(fread(json,1,size,input)==(size_t)size);
    json[size]=0;fclose(input);
    assert(tvg_engine_init(TVG_ENGINE_SW,0)==TVG_RESULT_SUCCESS);
    Tvg_Animation *animation=tvg_animation_new();assert(animation);
    Tvg_Paint *picture=tvg_animation_get_picture(animation);
    assert(tvg_picture_load_data(picture,json,size,"lottie",true)==TVG_RESULT_SUCCESS);
    float frames=0;assert(tvg_animation_get_total_frame(animation,&frames)==TVG_RESULT_SUCCESS);
    assert(frames==150);
    Tvg_Canvas *canvas=tvg_swcanvas_create();assert(canvas);
    assert(tvg_swcanvas_set_target(canvas,pixels,480,480,480,TVG_COLORSPACE_ARGB8888)==TVG_RESULT_SUCCESS);
    assert(tvg_picture_set_size(picture,480,480)==TVG_RESULT_SUCCESS);
    assert(tvg_canvas_push(canvas,picture)==TVG_RESULT_SUCCESS);
    for(unsigned frame=0;frame<150;frame++) {
        memset(pixels,0,sizeof(pixels));
        tvg_animation_set_frame(animation,(float)frame);
        assert(tvg_canvas_update(canvas)==TVG_RESULT_SUCCESS);
        assert(tvg_canvas_draw(canvas)==TVG_RESULT_SUCCESS);
        assert(tvg_canvas_sync(canvas)==TVG_RESULT_SUCCESS);
        char path[1024];snprintf(path,sizeof(path),"%s/frame_%03u.ppm",argv[2],frame);
        FILE *output=fopen(path,"wb");assert(output);fprintf(output,"P6\n480 480\n255\n");
        for(unsigned p=0;p<480*480;p++) {
            uint8_t rgb[3]={(pixels[p]>>16)&255,(pixels[p]>>8)&255,pixels[p]&255};
            fwrite(rgb,1,3,output);
        }
        fclose(output);
    }
    printf("Rendered %u frames, 30fps, five-second parameter study.\n",150U);
    /* Canvas owns picture; exiting standalone renderer releases all resources. */
    free(json);
    return 0;
}
