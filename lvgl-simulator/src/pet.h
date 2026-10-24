/**
 * @file pet.h
 * @brief 电子宠物核心模块（平台无关）
 *
 * 本模块只依赖 LVGL，不依赖任何平台（SDL 模拟器 / ESP-IDF 真机共用）。
 * 真机移植时仅需传入实际屏幕宽高，代码无需改动。
 *
 * 当前实现：清醒日常循环（窝坐 / 起身 / 发呆 / 散步 / 返回 / 窝下）。
 */
#ifndef PET_H
#define PET_H

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PET_STATE_IDLE,
    PET_STATE_WALK,
    PET_STATE_HUNGRY,
    PET_STATE_SLEEP,
} pet_state_t;

typedef enum { SLEEP_NONE, SLEEP_ENTER, SLEEP_LOOP, SLEEP_EXIT } pet_sleep_phase_t;
void pet_enter_sleep(void);
void pet_exit_sleep(void);
pet_sleep_phase_t pet_get_sleep_phase(void);

/**
 * @brief 创建电子宠物主界面
 * @param parent 父对象（模拟器传 lv_screen_active()，真机同样）
 * @param w 显示区宽度（模拟器与真机均为 410）
 * @param h 显示区高度（模拟器与真机均为 502）
 */
void pet_create(lv_obj_t *parent, lv_coord_t w, lv_coord_t h);

/** @brief IDLE resumes daily behavior; HUNGRY/SLEEP pause it until explicitly changed. */
void pet_set_state(pet_state_t state);

/** Play a 1600ms one-shot response; false while busy, sleeping or cooling down (2500ms). */
bool pet_play_happy_once(void);

/** Simulator preview: use the 6 AI happy keyframes (art-driven) during the
 *  response instead of bouncing one static base sprite. Off by default. */
void pet_set_happy_keyframes(bool on);

/** Simulator preview only: force-loop happy after gap_ms, bypassing cooldown. Not for firmware. */
void pet_happy_demo_tick(uint32_t gap_ms);

/** Simulator preview only: loop the angry keyframe sequence; per-frame holds + gap. Not for firmware. */
void pet_angry_demo_tick(uint32_t gap_ms);

/** Simulator preview workbench: freeze daily phase/walk/blink/mouth, show a neutral base. Not for firmware. */
void pet_set_demo_mode(bool on);

#ifdef __cplusplus
}
#endif

#endif /* PET_H */
