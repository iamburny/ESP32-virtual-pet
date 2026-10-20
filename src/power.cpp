#include "power.h"

#include <Arduino.h>

#include "display.h"

#define LEVEL_FULL 255
#define LEVEL_DIM 40
#define LEVEL_PULSE 90
#define DIM_AFTER_MS 30000
#define OFF_AFTER_MS 120000
#define PULSE_MS 2500

static uint32_t last_activity_ms;
static uint32_t pulse_start_ms;
static float level;
static uint8_t applied = 255;

void power_init() {
  last_activity_ms = millis();
  level = LEVEL_FULL;
}

void power_activity() {
  last_activity_ms = millis();
  pulse_start_ms = 0;
}

bool power_touch_filter() {
  bool was_off = power_screen_off();
  power_activity();
  return was_off;
}

void power_pulse() {
  if (power_screen_off()) pulse_start_ms = millis();
}

bool power_screen_off() { return millis() - last_activity_ms >= OFF_AFTER_MS; }
bool power_idle() { return millis() - last_activity_ms >= DIM_AFTER_MS; }

void power_update() {
  static uint32_t last_update_ms;
  uint32_t now = millis();
  if (now - last_update_ms < 20) return;  // fade rates below assume a fixed 50 Hz step
  last_update_ms = now;
  uint32_t idle = now - last_activity_ms;

  float target = idle < DIM_AFTER_MS ? LEVEL_FULL : idle < OFF_AFTER_MS ? LEVEL_DIM : 0;
  if (pulse_start_ms) {
    uint32_t t = now - pulse_start_ms;
    if (t < PULSE_MS)
      target = LEVEL_PULSE * sinf(PI * t / PULSE_MS);
    else
      pulse_start_ms = 0;
  }

  // Fade quickly up, slowly down.
  float rate = target > level ? 0.35f : 0.06f;
  level += (target - level) * rate;
  uint8_t out = (uint8_t)(level + 0.5f);
  if (out != applied) {
    applied = out;
    display_set_backlight(out);
  }
}
