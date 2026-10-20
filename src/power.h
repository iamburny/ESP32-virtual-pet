#pragma once

#include <stdint.h>

void power_init();
// Any user interaction: restores full brightness and restarts the idle timers.
void power_activity();
// Touch filter for lvgl_port: when the screen is off, the touch only wakes it.
bool power_touch_filter();
// Briefly glows the backlight while the screen is off, to signal a pet event.
void power_pulse();
// Drives idle dimming and backlight fades; call every loop().
void power_update();
bool power_screen_off();
// True once the screen has dimmed for lack of interaction.
bool power_idle();
