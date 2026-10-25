#pragma once

#include <stdint.h>

#include "hw/board.h"

// Implemented per board in src/boards/<board>/display.cpp.
void display_init();
void display_set_backlight(uint8_t duty);  // 0-255
