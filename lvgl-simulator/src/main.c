/**
 * @file main.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#ifndef _DEFAULT_SOURCE
  #define _DEFAULT_SOURCE /* needed for usleep() */
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef _MSC_VER
  #include <Windows.h>
#else
  #include <unistd.h>
  #include <pthread.h>
#endif
#include "lvgl/lvgl.h"
#include <SDL.h>

#include "lvgl/src/drivers/sdl/lv_sdl_private.h"

#include "hal/hal.h"
#include "pet.h"
#include "lottie_demo.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

#if LV_USE_OS != LV_OS_FREERTOS

/* Explicit visual demo: quiet awake -> enter -> quiet sleep -> exit, repeat. */
static void sleep_demo_cb(lv_timer_t *timer)
{
  (void)timer;
  static unsigned stage;
  static uint32_t since;
  static bool initialized;
  if(!initialized) { initialized=true;since=lv_tick_get(); }
  pet_sleep_phase_t phase=pet_get_sleep_phase();
  if(stage==0 && lv_tick_elaps(since)>=3000) {
    pet_enter_sleep();stage=1;
  } else if(stage==1 && phase==SLEEP_LOOP) {
    since=lv_tick_get();stage=2;
  } else if(stage==2 && lv_tick_elaps(since)>=5000) {
    pet_exit_sleep();stage=3;
  } else if(stage==3 && phase==SLEEP_NONE) {
    since=lv_tick_get();stage=0;
  }
}

/* Preview: loop the happy response with an 800ms rest gap; bypasses cooldown. */
static void happy_demo_cb(lv_timer_t *timer)
{
  (void)timer;
  pet_happy_demo_tick(800);
}

/* Preview: loop the angry keyframe sequence with a 700ms rest gap. */
static void angry_demo_cb(lv_timer_t *timer)
{
  (void)timer;
  pet_angry_demo_tick(700);
}
/* Offline frame dump: read the SDL renderer backbuffer and save a BMP.
   Usage: main.exe --shot <count> <every> <prefix>
     count  - total frames to save
     every  - save one frame every N rendered ticks (33ms per tick)
     prefix - output path prefix, files become prefix_000.bmp ... */
static void dump_sdl_frame(const char *path)
{
  lv_display_t * disp = lv_display_get_default();
  if(!disp) return;
  SDL_Renderer * ren = lv_sdl_backend_ops.get_renderer(disp);
  if(!ren) return;
  int w = lv_sdl_window_get_horizontal_resolution(disp);
  int h = lv_sdl_window_get_vertical_resolution(disp);
  SDL_Surface * s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ABGR8888);
  if(s) {
    if(SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ABGR8888, s->pixels, s->pitch) == 0)
      SDL_SaveBMP(s, path);
    SDL_FreeSurface(s);
  }
}

int main(int argc, char **argv)
{
  (void)argc; /*Unused*/
  (void)argv; /*Unused*/

#ifdef _WIN32
  /*MinGW-built SDL2 may silently fail on the Direct3D renderer (black window).
    Force the software renderer; 410x502 canvas is cheap to blit on CPU.*/
  SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
#endif

  /*Initialize LVGL*/
  lv_init();

  /*Bind LVGL tick to SDL time (otherwise tick stays 0 and no timer/refr ever fires)*/
  lv_tick_set_cb(SDL_GetTicks);

  /*Initialize the HAL (display, input devices, tick) for LVGL*/
  sdl_hal_init(410, 502);

  /* 启动电子宠物主界面（清醒日常循环 + 触摸开心） */
  pet_create(lv_screen_active(), 410, 502);
  if(argc>1 && strcmp(argv[1],"--sleep-demo")==0) lv_timer_create(sleep_demo_cb,100,NULL);
  if(argc>1 && strcmp(argv[1],"--happy-demo")==0) {
    pet_set_demo_mode(true); /* freeze daily behavior; loop only happy */
    lv_timer_create(happy_demo_cb,100,NULL);
  }
  if(argc>1 && strcmp(argv[1],"--angry-demo")==0) {
    pet_set_demo_mode(true); /* freeze daily behavior; loop only angry */
    lv_timer_create(angry_demo_cb,100,NULL);
  }
  /* Lottie 演示版（lv_lottie + ThorVG）备用：改调 lottie_demo_create 即可切换 */
  //lottie_demo_create(lv_screen_active(), 410, 502);

  /* --shot <count> <every> <prefix>: render, dump frames, exit */
  int shot_left = 0, shot_every = 5, shot_tick = 0;
  const char * shot_prefix = "frame";
  if(argc > 2 && strcmp(argv[1], "--shot") == 0) {
    shot_left = atoi(argv[2]);
    if(argc > 3) shot_every = atoi(argv[3]);
    if(shot_every < 1) shot_every = 1;
    if(argc > 4) shot_prefix = argv[4];
  }

  while(1) {
    /* Periodically call the lv_task handler.
     * It could be done in a timer interrupt or an OS task too.*/
    uint32_t sleep_time_ms = lv_timer_handler();
    if(sleep_time_ms == LV_NO_TIMER_READY){
	sleep_time_ms =  LV_DEF_REFR_PERIOD;
    }

    if(shot_left > 0) {
      if(shot_tick % shot_every == 0) {
        char path[600];
        snprintf(path, sizeof(path), "%s_%03d.bmp", shot_prefix, shot_tick / shot_every);
        dump_sdl_frame(path);
        if(--shot_left == 0) return 0;
      }
      shot_tick++;
    }

#ifdef _MSC_VER
    Sleep(sleep_time_ms);
#else
    usleep(sleep_time_ms * 1000);
#endif
  }

  return 0;
}


#endif

/**********************
 *   STATIC FUNCTIONS
 **********************/

