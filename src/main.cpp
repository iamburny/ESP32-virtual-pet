#include <Arduino.h>
#include <lvgl.h>

#include "app.h"
#include "ble/friends.h"
#include "display.h"
#include "hw/imu.h"
#include "lvgl_port.h"
#include "net/net_task.h"
#include "power.h"
#include "ui/ui.h"

#define FRAME_MS 33
#define FRAME_MS_SCREEN_OFF 200
#define STATS_EVERY_MS 10000

static lv_timer_t *frame_timer;
static uint32_t last_frame_ms, last_stats_ms;

static void frame_cb(lv_timer_t *) {
  uint32_t now = millis();
  uint32_t dt = now - last_frame_ms;
  last_frame_ms = now;
  ui_frame(dt);
  lv_timer_set_period(frame_timer, power_screen_off() ? FRAME_MS_SCREEN_OFF : FRAME_MS);
}

static void log_stats() {
  uint32_t frames, render_ms, px;
  lvgl_port_take_stats(frames, render_ms, px);
  float secs = STATS_EVERY_MS / 1000.0f;
  Serial.printf("[perf] %.1f fps, %lu ms/frame, %lu px/frame, heap %lu free (min %lu)\n", frames / secs,
                frames ? (unsigned long)(render_ms / frames) : 0, frames ? (unsigned long)(px / frames) : 0,
                (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap());
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("[boot] Tomo starting");

  display_init();
  lvgl_port_init();
  power_init();
  lvgl_port_set_touch_filter(power_touch_filter);
  imu_init();

  app_init();
  net_start();
  ui_init();
  friends_init(g_pet);

  last_frame_ms = last_stats_ms = millis();
  frame_timer = lv_timer_create(frame_cb, FRAME_MS, nullptr);
  Serial.printf("[boot] ready, heap %lu free\n", (unsigned long)ESP.getFreeHeap());
}

void loop() {
  static uint32_t prev_end = millis();  // logs loop stalls, e.g. other tasks hogging the single core
  uint32_t t0 = millis();
  uint8_t gestures = imu_update();
  if (gestures) {
    power_activity();
    ui_on_gestures(gestures);
  }
  uint32_t t1 = millis();
  app_loop();
  uint32_t t2 = millis();
  friends_tick();
  uint32_t t3 = millis();
  power_update();
  net_set_busy(!power_idle());
  lv_timer_handler();
  uint32_t t4 = millis();
  if (t4 - prev_end > 150)
    Serial.printf("[hitch] gap %lu imu %lu app %lu ble %lu lvgl %lu\n", (unsigned long)(t0 - prev_end),
                  (unsigned long)(t1 - t0), (unsigned long)(t2 - t1), (unsigned long)(t3 - t2), (unsigned long)(t4 - t3));
  prev_end = millis();

  if (millis() - last_stats_ms >= STATS_EVERY_MS) {
    last_stats_ms = millis();
    log_stats();
  }
#ifdef IMU_DEBUG
  static uint32_t imu_dbg_ms;
  if (millis() - imu_dbg_ms >= 1000) {
    imu_dbg_ms = millis();
    const ImuState &st = imu_state();
    Serial.printf("[imu] g=(%.2f, %.2f, %.2f) motion=%.2f\n", st.gx, st.gy, st.gz, st.motion);
  }
#endif
  delay(2);
}
