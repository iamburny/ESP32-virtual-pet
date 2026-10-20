#pragma once

#include <Wire.h>

// Touch controller (AXS5106L, 0x63) and IMU (QMI8658, 0x6B) share this bus.
#define I2C_SDA 18
#define I2C_SCL 19

#define TOUCH_RST 20
#define TOUCH_INT 21

inline void hw_i2c_init() {
  static bool done = false;
  if (done) return;
  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  done = true;
}
