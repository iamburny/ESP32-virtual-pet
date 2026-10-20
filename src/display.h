#pragma once

#include <Arduino_GFX_Library.h>

#define DISPLAY_WIDTH 172
#define DISPLAY_HEIGHT 320

extern Arduino_GFX *gfx;

void display_init();
void display_set_backlight(uint8_t duty); // 0-255
