#pragma once

#include <stdint.h>

void lvgl_port_init();

// Called on each new touch press; returning true hides that press from LVGL
// (used to let the first touch on a dark screen only wake it).
typedef bool (*TouchFilter)();
void lvgl_port_set_touch_filter(TouchFilter filter);

// Refresh statistics since the last call: frames drawn, total time spent
// rendering + flushing them, and pixels refreshed.
void lvgl_port_take_stats(uint32_t &frames, uint32_t &render_ms, uint32_t &pixels);

// Number of display refreshes completed since boot.
uint32_t lvgl_port_refresh_count();
