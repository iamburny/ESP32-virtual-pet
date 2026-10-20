#pragma once

#include <stdint.h>

// Gravity and motion in screen coordinates, in g: +x towards the screen's
// right edge, +y towards its bottom edge, +z out of the screen towards the viewer.
struct ImuState {
  bool ok;
  float gx, gy, gz;  // low-passed gravity direction
  float motion;      // smoothed magnitude of non-gravity acceleration
};

enum : uint8_t {
  IMU_SHAKE = 1 << 0,
  IMU_TAP = 1 << 1,
  IMU_PICKUP = 1 << 2,
  IMU_FACE_DOWN = 1 << 3,
};

bool imu_init();
// Reads any pending samples; returns the gestures detected since the last call.
uint8_t imu_update();
const ImuState &imu_state();
