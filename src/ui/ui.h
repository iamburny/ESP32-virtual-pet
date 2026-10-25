#pragma once

#include <stdint.h>

#include "ble/friends.h"
#include "render/icons.h"
#include "render/pet_view.h"

void ui_init();
// Per-frame update for whatever screen is showing.
void ui_frame(uint32_t dt_ms);
// Reacts to simulation events (hatching, evolving, sickness, ...).
void ui_on_events(uint32_t events);
void ui_on_gestures(uint8_t gestures);
void ui_toast(const char *msg, Icon icon, uint32_t color);
// A pal has come into range.
void ui_pal_nearby(const FriendSeen &f);
// The invited pal walks on, refreshes its look from a new sighting, or walks off.
void ui_guest_arrive(const FriendSeen &f, uint32_t gift, bool first_time);
void ui_guest_update(const FriendSeen &f);
void ui_guest_leave();
