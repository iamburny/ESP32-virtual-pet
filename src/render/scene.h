#pragma once

#include <lvgl.h>

#define SCENE_GROUND_Y 232  // top of the grass
#define SCENE_FEET_Y 266    // where the pet stands

// Back layer (sky, sun/moon, stars, clouds, ground, poop) and front layer
// (rain/snow/fog, lightning, lights-off darkness). Create the back layer
// before the pet and the front layer after it.
lv_obj_t *scene_create_back(lv_obj_t *parent);
lv_obj_t *scene_create_front(lv_obj_t *parent);

void scene_set_clock(int hour, int minute);
void scene_set_weather(uint8_t weather);  // Weather from net_task.h; WX_UNKNOWN = clear
void scene_set_lights(bool on);
void scene_set_poop(int count);
// Index of the poop under the screen point, or -1.
int scene_poop_at(int x, int y);
void scene_tick(uint32_t dt_ms);

// 0 in full daylight, 1 at night; other UI uses it to tint itself.
float scene_night();
