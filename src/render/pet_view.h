#pragma once

#include <lvgl.h>

#include "pet/pet_sim.h"

// One-shot animations layered over the mood-driven idle behaviour.
enum PetAnim : uint8_t {
  ANIM_NONE,
  ANIM_HAPPY,      // hops + hearts
  ANIM_PAT,        // squish + heart
  ANIM_EAT,        // chews the food set by pet_view_set_food()
  ANIM_REFUSE,     // shakes head
  ANIM_DIZZY,      // spinning eyes after a shake
  ANIM_SURPRISED,  // picked up
  ANIM_DANCE,      // celebrations, friend visits
  ANIM_MEDICINE,   // grimace
  ANIM_HATCH,      // egg bursts open
  ANIM_WAKE,       // jolted awake
};

enum : uint8_t {
  ACC_UMBRELLA = 1 << 0,
  ACC_SUNGLASSES = 1 << 1,
  ACC_SCARF = 1 << 2,
};

// Creates a pet whose body is roughly `size` px wide. The object is larger than
// the body to leave room for ears, jumps and particles; its feet sit near the
// bottom edge, centred horizontally.
lv_obj_t *pet_view_create(lv_obj_t *parent, int size);

void pet_view_set_look(lv_obj_t *obj, uint8_t form, uint8_t palette, uint8_t stage);
void pet_view_set_mood(lv_obj_t *obj, Mood mood);
void pet_view_set_accessories(lv_obj_t *obj, uint8_t acc);
void pet_view_set_egg_progress(lv_obj_t *obj, float progress);  // 0..1, cracks near the end
void pet_view_set_food(lv_obj_t *obj, Food food);
void pet_view_play(lv_obj_t *obj, PetAnim anim);
bool pet_view_busy(lv_obj_t *obj);

// Screen-space point for the eyes to follow; pass false to resume looking around.
void pet_view_look_at(lv_obj_t *obj, bool active, int x = 0, int y = 0);
// -1..1 body lean (from device tilt).
void pet_view_set_lean(lv_obj_t *obj, float lean);
void pet_view_set_mirror(lv_obj_t *obj, bool facing_left);

// Advances animation by dt and invalidates the object.
void pet_view_tick(lv_obj_t *obj, uint32_t dt_ms);

// True if the screen point is on the pet's body.
bool pet_view_hit(lv_obj_t *obj, int x, int y);

void pet_view_emit_hearts(lv_obj_t *obj, int count);
void pet_view_emit_sparkles(lv_obj_t *obj, int count);

// Palette colours, shared with UI elements that echo the pet's colour.
lv_color_t pet_palette_main(uint8_t palette);
lv_color_t pet_palette_light(uint8_t palette);
