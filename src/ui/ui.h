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
// True when the home screen is showing with nothing open over it.
bool ui_idle_on_home();
void ui_friend_visit(const FriendSeen &f, uint32_t gift, bool first_time);
