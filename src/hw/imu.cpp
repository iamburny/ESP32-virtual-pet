#include "imu.h"

#include "board.h"

#if BOARD_HAS_IMU

#include <Arduino.h>
#include <SensorQMI8658.hpp>
#include <math.h>

#define QMI8658_ADDR 0x6B

// Maps the chip's raw accelerometer axes to screen-space gravity (measured on
// the board: held upright it reads ay = -1 g, right edge down ax = -1 g, face
// up az = -1 g).
static inline void raw_to_screen(float ax, float ay, float az, float &x, float &y, float &z) {
  x = -ax;
  y = -ay;
  z = az;
}

static SensorQMI8658 qmi;
static ImuState st;

static float lp_x, lp_y, lp_z;
static uint32_t peak_times[4];
static uint8_t peak_count;
static bool in_peak;
static uint32_t last_shake_ms;
static uint32_t pending_tap_ms;
static uint32_t still_since_ms;
static uint32_t face_down_since_ms;
static bool face_down_reported;

bool imu_init() {
  hw_i2c_init();
  if (!qmi.begin(Wire, QMI8658_ADDR)) {
    Serial.println("[imu] QMI8658 not found");
    st.ok = false;
    return false;
  }
  qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_125Hz, SensorQMI8658::LPF_MODE_0);
  qmi.enableAccelerometer();
  st.ok = true;
  st.gz = -1;
  lp_z = -1;
  still_since_ms = millis();
  Serial.printf("[imu] QMI8658 id=0x%02X\n", qmi.getChipID());
  return true;
}

uint8_t imu_update() {
  if (!st.ok) return 0;
  uint8_t gestures = 0;
  uint32_t now = millis();

  for (int i = 0; i < 4 && qmi.getDataReady(); i++) {
    float ax, ay, az;
    if (!qmi.getAccelerometer(ax, ay, az)) break;
    float x, y, z;
    raw_to_screen(ax, ay, az, x, y, z);

    const float k = 0.08f;
    lp_x += (x - lp_x) * k;
    lp_y += (y - lp_y) * k;
    lp_z += (z - lp_z) * k;

    float dx = x - lp_x, dy = y - lp_y, dz = z - lp_z;
    float d = sqrtf(dx * dx + dy * dy + dz * dz);
    st.motion += (d - st.motion) * 0.05f;

    // Peaks of non-gravity acceleration: several in quick succession is a shake,
    // an isolated one is a tap.
    if (!in_peak && d > 0.9f) {
      in_peak = true;
      if (peak_count < 4) peak_times[peak_count++] = now;
      pending_tap_ms = now;
    } else if (in_peak && d < 0.4f) {
      in_peak = false;
    }
  }

  while (peak_count && now - peak_times[0] > 900) {
    for (int i = 1; i < peak_count; i++) peak_times[i - 1] = peak_times[i];
    peak_count--;
  }
  if (peak_count >= 3 && now - last_shake_ms > 1500) {
    gestures |= IMU_SHAKE;
    last_shake_ms = now;
    peak_count = 0;
    pending_tap_ms = 0;
  }
  if (pending_tap_ms && now - pending_tap_ms > 350) {
    if (peak_count == 1 && now - last_shake_ms > 1500) gestures |= IMU_TAP;
    pending_tap_ms = 0;
  }

  // Pick-up: sustained motion after the board has been still for a while.
  if (st.motion < 0.03f) {
    if (!still_since_ms) still_since_ms = now;
  } else if (st.motion > 0.12f) {
    if (still_since_ms && now - still_since_ms > 3000) gestures |= IMU_PICKUP;
    still_since_ms = 0;
  }

  // Face-down: screen towards the floor for 1.5 s, reported once per flip.
  if (lp_z > 0.85f) {
    if (!face_down_since_ms) face_down_since_ms = now;
    if (!face_down_reported && now - face_down_since_ms > 1500) {
      gestures |= IMU_FACE_DOWN;
      face_down_reported = true;
    }
  } else {
    face_down_since_ms = 0;
    face_down_reported = false;
  }

  st.gx = lp_x;
  st.gy = lp_y;
  st.gz = lp_z;
  return gestures;
}

const ImuState &imu_state() { return st; }

#else  // no IMU on this board: tilt and motion gestures are simply absent

static ImuState st;
bool imu_init() { return false; }
uint8_t imu_update() { return 0; }
const ImuState &imu_state() { return st; }

#endif
