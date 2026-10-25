#pragma once

#include <lvgl.h>

#include "render/pet_view.h"

// Shared between the home screen, its sheets, and the mini-games.

lv_obj_t *home_screen();
lv_obj_t *home_pet();
lv_obj_t *home_top_layer();  // parent for sheets, above the scene and HUD
void home_play(PetAnim anim);
void home_refresh_hud();
// Returns to the home screen (after a game), with a fade.
void home_show();

// Sheets (menu drawer, feed, play, stats, friends) on the home screen.
void sheets_init(lv_obj_t *layer);
void sheets_open_menu();
// "Are you sure?" dialog for replacing the pet with a new egg.
void sheets_open_start_over();
void sheets_close_all();
bool sheets_any_open();
void sheets_frame(uint32_t dt_ms);

// Mini-games run on their own screen.
void game_start_catch();
void game_start_bubble();
bool game_active();
void game_frame(uint32_t dt_ms);
