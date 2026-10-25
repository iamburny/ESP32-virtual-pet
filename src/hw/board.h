#pragma once

#include <Wire.h>

// Per-board hardware constants. Each PlatformIO env defines one BOARD_* macro and
// compiles the matching driver folder under src/boards/.

#if defined(BOARD_JC3248W535)
// Guition JC3248W535EN: ESP32-S3, 320x480 AXS15231B over QSPI, AXS15231 touch @ 0x3B.
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 480
#define BOARD_HAS_IMU 0
#define I2C_SDA 4
#define I2C_SCL 8

#elif defined(BOARD_C6_LCD147)
// Waveshare ESP32-C6-Touch-LCD-1.47: 172x320 JD9853, AXS5106L touch (0x63) and
// QMI8658 IMU (0x6B) on the same bus.
#define DISPLAY_WIDTH 172
#define DISPLAY_HEIGHT 320
#define BOARD_HAS_IMU 1
#define I2C_SDA 18
#define I2C_SCL 19
#define TOUCH_RST 20
#define TOUCH_INT 21

#else
#error "Define a BOARD_* macro (see platformio.ini)"
#endif

inline void hw_i2c_init() {
  static bool done = false;
  if (done) return;
  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  done = true;
}
