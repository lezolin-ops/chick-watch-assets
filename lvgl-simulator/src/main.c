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
int main(int argc, char **argv)
{
  (void)argc; /*Unused*/
  (void)argv; /*Unused*/

  /*Initialize LVGL*/
  lv_init();

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

  while(1) {
    /* Periodically call the lv_task handler.
     * It could be done in a timer interrupt or an OS task too.*/
    uint32_t sleep_time_ms = lv_timer_handler();
    if(sleep_time_ms == LV_NO_TIMER_READY){
	sleep_time_ms =  LV_DEF_REFR_PERIOD;
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

