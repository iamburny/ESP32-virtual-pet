#pragma once

#include "ble/friends.h"
#include "pet/pet_save.h"
#include "pet/pet_sim.h"

extern PetState g_pet;
extern FriendRecord g_friends[MAX_FRIENDS];
extern int g_friend_count;

void app_init();
// Advances the simulation, handles clock sync, saving and BLE sightings.
void app_loop();

// Simulation wall-clock second (real time once NTP has synced).
uint32_t app_epoch();
bool app_clock_synced();
int app_local_hour(uint32_t epoch);
// Schedules a save a few seconds from now.
void app_mark_dirty();

// Player actions: apply to the pet, save, and drive the UI reaction.
bool app_feed(Food food);
bool app_clean();
bool app_medicine();
void app_toggle_light();
void app_pat();
void app_shaken();
void app_game_over(uint32_t score);
