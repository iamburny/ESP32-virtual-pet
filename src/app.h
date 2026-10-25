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
// Other Tomos heard over BLE recently and within a few metres.
int app_nearby_count();
const FriendSeen &app_nearby(int i);
// Pet id of the nearby pal currently on screen, or 0.
uint32_t app_guest_id();
// Brings a nearby pal onto the home screen until it goes out of range (or
// another is invited). Records the friendship and may give a gift. False if
// the pal is gone or this pet can't have guests (egg, asleep).
bool app_invite(uint32_t pet_id);
void app_send_guest_home();

// Replaces the pet with a brand-new egg (stats and coins reset; pals are kept).
void app_start_over();
