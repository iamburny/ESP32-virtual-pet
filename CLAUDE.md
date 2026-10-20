# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

"Tomo": a Tamagotchi-style virtual pet firmware (PlatformIO, Arduino core 3.x, LVGL 8.4) for the Waveshare **ESP32-C6-Touch-LCD-1.47**: 172×320 JD9853 panel, AXS5106L touch (I²C 0x63), QMI8658 IMU (I²C 0x6B, same bus, SDA 18 / SCL 19). No PSRAM, so watch RAM. The previous "now playing" firmware is kept in `backups/now-playing/` (git-ignored) for reference only.

## Commands

PlatformIO lives at `~/.platformio/penv/Scripts/pio` (may not be on PATH). The board enumerates as **COM7** (set in `platformio.ini`).

```sh
export PYTHONIOENCODING=utf-8 PYTHONUTF8=1   # required when output is piped: esptool's progress bar crashes on cp1252 and the upload hangs
pio run                         # build (default env esp32-c6-touch-lcd-147)
pio run -t upload               # build + flash
pio device monitor              # serial @ 115200
pio test -e native              # host unit tests for src/pet/pet_sim.cpp (uses the WinLibs gcc on PATH)
pio test -e native -f test_pet_sim
PLATFORMIO_BUILD_FLAGS=-DIMU_DEBUG pio run -t upload   # prints IMU gravity once a second
```

Serial logs to look for: `[perf]` every 10 s (fps, ms/frame, px/frame, free heap), `[hitch]` when a loop iteration takes >150 ms (with per-step timings; a large `gap` means another task held the single core), `[app]` sim events, `[net]`, `[ble]`, `[imu]`.

## Configuration

`src/secrets.h` (git-ignored) defines `WIFI_SSID`, `WIFI_PASS`, `WEATHER_LAT`, `WEATHER_LON` (strings), and `TZ_POSIX`. Don't echo its values.

## Architecture

**Threads.** LVGL is only touched from the Arduino `loop()` task. `net/net_task.cpp` is a FreeRTOS task that periodically brings Wi-Fi up, syncs NTP, fetches Open-Meteo weather over plain HTTP (HTTP/1.0 to avoid chunked bodies), and turns Wi-Fi off again; results go through a mutex (`net_get()`). Bringing Wi-Fi up stalls the UI for ~1.7 s, so after the first sync the task waits until the screen has dimmed (`net_set_busy(!power_idle())`, at most 15 min). `ble/friends.cpp` advertises a 19-byte manufacturer-data beacon (company 0xFFFF, "TM") and runs a 5 s passive NimBLE scan every minute; sightings go through a FreeRTOS queue (`friends_poll()`).

**Frame loop.** `main.cpp` runs `imu_update()`, `app_loop()`, `friends_tick()`, `power_update()`, `lv_timer_handler()`. A 33 ms LVGL timer calls `ui_frame(dt)` (200 ms when the screen is off), which ticks either the home screen or the active mini-game.

**Game logic** (`pet/pet_sim.*`) is pure C++ with no Arduino/LVGL includes so it builds on the host. `pet_sim_advance()` steps in fixed 60 s increments (catch-up capped at 7 days), so the result is identical however often it's called. Keep it deterministic and keep the native tests passing. The persisted `PetState` is a raw struct in a versioned, CRC'd `PetBlob` — **bump `PET_STATE_VERSION` whenever `PetState`'s layout changes** (old saves are then discarded and a new egg starts).

**Clock** (`app.cpp`). A fresh pet runs on a relative epoch (starts at 1000) until NTP arrives, then is shifted onto real time without catch-up. A pet that has seen real time catches up on boot once NTP syncs. `app_local_hour()` treats pre-sync epochs as midday.

**Rendering** is procedural: no bitmaps. `render/draw2d.h` wraps LVGL 8 draw primitives (`lv_draw_rect/arc/line/polygon`) for use inside `LV_EVENT_DRAW_MAIN` handlers. `render/pet_view.cpp` is a custom LVGL object drawing the pet (per-form body/ears/accessories, expressions, spring-based squash/lean, particles) and is reused for the friend visitor and in games. `render/scene.cpp` draws two full-screen layers: back (sky by hour, sun/moon, stars, clouds, ground, poop) and front (rain/snow/fog, lightning, lights-off dimming). `render/icons.cpp` draws all icons as vectors on a 16-unit grid.

**Invalidation matters for fps.** The SPI bus runs at 40 MHz (GPIO-matrix pins) with a synchronous flush, so a full-screen redraw costs ~22 ms of transfer alone. Animated things invalidate only their own area: the pet invalidates its object, rain/snow drops invalidate per-drop strips, and clouds redraw a few times a second. Keep it that way and check `[perf]` after visual changes. LVGL keeps 32 invalid areas before falling back to a full redraw.

**LVGL gotchas hit here:** sizes are computed lazily, so call `lv_obj_update_layout()` before reading a new object's size (`pet_view_create` does). Delete sheets with `lv_obj_del_async` because they are closed from their own click handlers. The built-in Montserrat fonts include `°` and `•` but not `·`. Fonts enabled: Montserrat 12/14/16/20/28/48.

**IMU axes** (`hw/imu.cpp` `raw_to_screen`) were measured on the board: screen x = −ax, y = −ay, z = +az (gravity reads z = −1 face up).

**UI** (`ui/`). `home.cpp` owns the home screen (touch: stroke the pet to pat it, tap poop to clean it, swipe up or tap the bottom edge for the menu; reactions to IMU gestures; toasts; friend visits). `sheets.cpp` has the menu drawer, feed shop, play picker, stats, and pals list. `games/games.cpp` has Treat Catch (tilt or drag) and Bubble Pop, each on its own screen; `home_show()` fades back and auto-deletes the game screen.
