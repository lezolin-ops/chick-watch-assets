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

/* Preview the FIRMWARE response (same code a tap runs on the real device)
   in a loop. Gap must exceed the 2500ms cooldown inside pet_play_happy_once. */
static void happy_once_loop_cb(lv_timer_t *timer)
{
  (void)timer;
  pet_play_happy_once();
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

  /* 启动电子宠物主界面（清醒日常循环 + 触摸开心）
   * Demo flags and --shot may combine in any order, e.g.:
   *   main.exe --happy-demo --shot 80 2 E:/tmp/f  */
  pet_create(lv_screen_active(), 410, 502);
  bool happy_demo=false, angry_demo=false, sleep_demo=false;
  uint32_t happy_once_at = 0;
  /* --shot <count> <every> <prefix>: render, dump frames, exit.
   * Driven by LVGL tick (real time), NOT loop iterations: iterations vary
   * in length (timer handler returns the wait till the next due timer).
   * every = frames per 33ms tick period, i.e. every=1 -> one frame each
   * ~33ms; every=2 -> one frame each ~66ms. */
  int shot_left = 0, shot_every = 5, shot_tick = 0;
  const char * shot_prefix = "frame";
  for(int i=1;i<argc;i++) {
    if(strcmp(argv[i],"--sleep-demo")==0) sleep_demo=true;
    else if(strcmp(argv[i],"--happy-demo")==0) happy_demo=true;
    else if(strcmp(argv[i],"--angry-demo")==0) angry_demo=true;
    /* --happy-once [delay_ms]: fire the FIRMWARE happy response once -- 6
     * keyframes + programmatic squash/stretch over 1600ms, exactly what a
     * tap runs on the real device. Uses no AI sequence frames. */
    else if(strcmp(argv[i],"--happy-once")==0) {
      happy_once_at = (i+1<argc && argv[i+1][0]>='0' && argv[i+1][0]<='9')
                      ? (uint32_t)atoi(argv[++i]) : 500;
    }
    /* --happy-loop [gap_ms]: repeat the firmware response forever (visual
       review of the real-device animation path). */
    else if(strcmp(argv[i],"--happy-loop")==0) {
      uint32_t gap = (i+1<argc && argv[i+1][0]>='0' && argv[i+1][0]<='9')
                     ? (uint32_t)atoi(argv[++i]) : 2600;
      lv_timer_create(happy_once_loop_cb, gap < 2600 ? 2600 : gap, NULL);
    }
    else if(strcmp(argv[i],"--shot")==0 && i+1<argc) {
      shot_left = atoi(argv[++i]);
      if(i+1<argc) shot_every = atoi(argv[++i]);
      if(shot_every < 1) shot_every = 1;
      if(i+1<argc) shot_prefix = argv[++i];
    }
  }
  if(sleep_demo) lv_timer_create(sleep_demo_cb,100,NULL);
  if(happy_demo) {
    pet_set_demo_mode(true); /* freeze daily behavior; loop only happy */
    lv_timer_create(happy_demo_cb,100,NULL);
  }
  if(angry_demo) {
    pet_set_demo_mode(true); /* freeze daily behavior; loop only angry */
    lv_timer_create(angry_demo_cb,100,NULL);
  }
  /* Lottie 演示版（lv_lottie + ThorVG）备用：改调 lottie_demo_create 即可切换 */
  //lottie_demo_create(lv_screen_active(), 410, 502);

  /* Fire the firmware happy response once the delay elapses (fails silently
   * if the pet is not awake yet at that moment). */
  if(happy_once_at) happy_once_at += lv_tick_get();

  while(1) {
    /* Periodically call the lv_task handler.
     * It could be done in a timer interrupt or an OS task too.*/
    uint32_t sleep_time_ms = lv_timer_handler();
    if(sleep_time_ms == LV_NO_TIMER_READY){
	sleep_time_ms =  LV_DEF_REFR_PERIOD;
    }

    if(happy_once_at && lv_tick_get() >= happy_once_at) {
      happy_once_at = 0;
      if(!pet_play_happy_once()) fprintf(stderr,"[warn] happy-once rejected (not awake / cooling down)\n");
    }

    if(shot_left > 0) {
      /* Frame cadence follows LVGL tick (33ms per unit), not loop speed. */
      static uint32_t last_dump_tick = 0;
      uint32_t now = lv_tick_get();
      if(shot_tick == 0 || now - last_dump_tick >= (uint32_t)shot_every * 33) {
        char path[600];
        snprintf(path, sizeof(path), "%s_%03d.bmp", shot_prefix, shot_tick);
        dump_sdl_frame(path);
        last_dump_tick = now;
        shot_tick++;
        if(--shot_left == 0) return 0;
      }
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

