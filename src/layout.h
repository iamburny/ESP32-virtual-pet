#pragma once

#include <lvgl.h>
#include <math.h>

#include "hw/board.h"

// Layouts are designed in pixels for the 172x320 panel. S() scales sizes and
// vertical positions by screen height; SX() scales horizontal positions by width,
// so wider screens spread out rather than leaving empty margins.
#define UI_SCALE (DISPLAY_HEIGHT / 320.0f)
#define UI_SCALE_X (DISPLAY_WIDTH / 172.0f)

static inline int S(float px) { return (int)lroundf(px * UI_SCALE); }
static inline float Sf(float px) { return px * UI_SCALE; }
static inline int SX(float px) { return (int)lroundf(px * UI_SCALE_X); }

// Font roles, named by their size on the 172x320 panel.
#if DISPLAY_HEIGHT >= 480
#define FONT_12 (&lv_font_montserrat_18)
#define FONT_14 (&lv_font_montserrat_20)
#define FONT_16 (&lv_font_montserrat_24)
#define FONT_20 (&lv_font_montserrat_28)
#define FONT_28 (&lv_font_montserrat_40)
#define FONT_48 (&lv_font_montserrat_48)
#else
#define FONT_12 (&lv_font_montserrat_12)
#define FONT_14 (&lv_font_montserrat_14)
#define FONT_16 (&lv_font_montserrat_16)
#define FONT_20 (&lv_font_montserrat_20)
#define FONT_28 (&lv_font_montserrat_28)
#define FONT_48 (&lv_font_montserrat_48)
#endif
