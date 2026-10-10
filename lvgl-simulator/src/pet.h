/**
 * @file pet.h
 * @brief 电子宠物核心模块（平台无关）
 *
 * 本模块只依赖 LVGL，不依赖任何平台（SDL 模拟器 / ESP-IDF 真机共用）。
 * 真机移植时仅需传入实际屏幕宽高，代码无需改动。
 *
 * 当前实现：P0 动画（呼吸 / 心跳 / 眨眼 / 待机微动）和 192 像素透明小黄鸡贴图。
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
    PET_STATE_HAPPY,
    PET_STATE_HUNGRY,
    PET_STATE_SLEEP,
} pet_state_t;

/**
 * @brief 创建电子宠物主界面
 * @param parent 父对象（模拟器传 lv_screen_active()，真机同样）
 * @param w 显示区宽度（模拟器与真机均为 410）
 * @param h 显示区高度（模拟器与真机均为 502）
 */
void pet_create(lv_obj_t *parent, lv_coord_t w, lv_coord_t h);

/** @brief Switch the active pet animation state. */
void pet_set_state(pet_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* PET_H */
