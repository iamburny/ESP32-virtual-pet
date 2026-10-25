# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

"Tomo": a Tamagotchi-style virtual pet firmware (PlatformIO, Arduino core 3.x, LVGL 8.4) for two boards, one PlatformIO env each:

| Env | Board | Notes |
|---|---|---|
| `esp32-c6-touch-lcd-147` (default) | Waveshare ESP32-C6-Touch-LCD-1.47 | 172×320 JD9853 over SPI, AXS5106L touch (0x63), QMI8658 IMU (0x6B), shared I²C SDA 18 / SCL 19. No PSRAM — watch RAM. COM7. |
| `jc3248w535` | Guition JC3248W535EN (ESP32-S3) | 320×480 AXS15231B over QSPI, AXS15231 touch (0x3B, SDA 4 / SCL 8, no reset line), 8 MB OPI PSRAM, **no IMU**. COM9. Same panel as `../solar` and `../van-switch-panel`. |

`USER_GUIDE.md` is the player-facing guide; keep it in step with gameplay changes. The previous "now playing" firmware is kept in `backups/now-playing/` (git-ignored) for reference only.

## Commands

PlatformIO lives at `~/.platformio/penv/Scripts/pio` (may not be on PATH). COM ports are set per env in `platformio.ini`; check with `pio device list` if a board re-enumerates.

```sh
export PYTHONIOENCODING=utf-8 PYTHONUTF8=1   # required when output is piped: esptool's progress bar crashes on cp1252 and the upload hangs
pio run                         # build (default env esp32-c6-touch-lcd-147)
pio run -t upload               # build + flash the C6
pio run -e jc3248w535 -t upload # build + flash the S3
pio device monitor              # serial @ 115200
pio test -e native              # host unit tests for src/pet/pet_sim.cpp (uses the WinLibs gcc on PATH)
pio test -e native -f test_pet_sim
PLATFORMIO_BUILD_FLAGS=-DIMU_DEBUG pio run -t upload   # prints IMU gravity once a second
```

Serial logs to look for: `[perf]` every 10 s (fps, ms/frame, px/frame, free heap), `[hitch]` when a loop iteration takes >150 ms (with per-step timings; a large `gap` means another task held the single core), `[app]` sim events, `[net]`, `[ble]`, `[imu]`.

## Configuration

`src/secrets.h` (git-ignored) defines `WIFI_SSID`, `WIFI_PASS`, `WEATHER_LAT`, `WEATHER_LON` (strings), and `TZ_POSIX`. Don't echo its values.

## Architecture

**Boards.** `hw/board.h` selects per-board constants (`DISPLAY_WIDTH/HEIGHT`, I²C pins, `BOARD_HAS_IMU`) from the env's `BOARD_*` macro. Each env compiles only its own `src/boards/<board>/` folder (`build_src_filter`), which implements `display.h` (`display_init`, backlight) and `lvgl_port.h` (LVGL display/touch glue, touch filter, refresh stats). Everything else is shared. Without an IMU, `hw/imu.cpp` compiles to stubs that report no IMU, and tilt/shake features simply never trigger.

**Resolution independence** (`layout.h`). Layouts are written in pixels for 172×320: wrap sizes and vertical positions in `S()`/`Sf()` (scale by height) and horizontal positions in `SX()` (scale by width); use the `FONT_12`…`FONT_48` roles, which map to larger Montserrat sizes on the 480-high panel. `pet_view.cpp` instead scales its detail offsets by the pet's own size with `U()`/`UP()` (designed for a 74 px pet). New UI code should never use raw pixel literals.

**Threads.** LVGL is only touched from the Arduino `loop()` task. `net/net_task.cpp` is a FreeRTOS task that periodically brings Wi-Fi up, syncs NTP, fetches Open-Meteo weather over plain HTTP (HTTP/1.0 to avoid chunked bodies), and turns Wi-Fi off again; results go through a mutex (`net_get()`). Bringing Wi-Fi up stalls the UI for ~1.7 s, so after the first sync the task waits until the screen has dimmed (`net_set_busy(!power_idle())`, at most 15 min). `ble/friends.cpp` advertises a 19-byte manufacturer-data beacon (company 0xFFFF, "TM") and runs a 5 s passive NimBLE scan every minute (every 20 s while a guest is over); sightings go through a FreeRTOS queue (`friends_poll()`). `app.cpp` keeps them as a "nearby" list that expires pals not heard for a couple of scans; inviting one from Pals (`app_invite`) puts it on the home screen as a guest until it drops out of range.

**Frame loop.** `main.cpp` runs `imu_update()`, `app_loop()`, `friends_tick()`, `power_update()`, `lv_timer_handler()`. A 33 ms LVGL timer calls `ui_frame(dt)` (200 ms when the screen is off), which ticks either the home screen or the active mini-game.

**Game logic** (`pet/pet_sim.*`) is pure C++ with no Arduino/LVGL includes so it builds on the host. `pet_sim_advance()` steps in fixed 60 s increments (catch-up capped at 7 days), so the result is identical however often it's called. Keep it deterministic and keep the native tests passing. The persisted `PetState` is a raw struct in a versioned, CRC'd `PetBlob` — **bump `PET_STATE_VERSION` whenever `PetState`'s layout changes** (old saves are then discarded and a new egg starts).

**Clock** (`app.cpp`). A fresh pet runs on a relative epoch (starts at 1000) until NTP arrives, then is shifted onto real time without catch-up. A pet that has seen real time catches up on boot once NTP syncs. `app_local_hour()` treats pre-sync epochs as midday.

**Rendering** is procedural: no bitmaps. `render/draw2d.h` wraps LVGL 8 draw primitives (`lv_draw_rect/arc/line/polygon`) for use inside `LV_EVENT_DRAW_MAIN` handlers. `render/pet_view.cpp` is a custom LVGL object drawing the pet (per-form body/ears/accessories, expressions, spring-based squash/lean, particles) and is reused for the friend visitor and in games. `render/scene.cpp` draws two full-screen layers: back (sky by hour, sun/moon, stars, clouds, ground, poop) and front (rain/snow/fog, lightning, lights-off dimming). `render/icons.cpp` draws all icons as vectors on a 16-unit grid.

**C6 render path.** The SPI bus runs at 40 MHz (GPIO-matrix pins) with a synchronous flush of 64-line strips, so a full-screen redraw costs ~22 ms of transfer alone; only animated areas are redrawn (see below). Check `[perf]` after visual changes.

**S3 render path** (`boards/jc3248w535/lvgl_port.cpp`). The AXS15231B only accepts whole frames — sending just the changed rows was tried and garbles it. LVGL runs in **double-buffered direct mode** on two full-screen PSRAM buffers (its built-in sync copies the previous frame's dirty areas across), and each finished frame is handed to a **push task on core 0** (~22 ms per 300 KB frame) while core 1 renders the next. The flush reports ready immediately; the next hand-over blocks on the previous push, so a buffer is never overwritten mid-send. Keep: `LV_COLOR_16_SWAP 1` for this board only (frames go out with no per-pixel swap via `draw16bitBeRGBBitmap`); `ESP32QSPI_MAX_PIXELS_AT_ONCE=4096` (8192 used too much internal DMA RAM); **QSPI at 40 MHz** (80 MHz smears the image). Never rotate the AXS15231B driver. Tried and measured slower: rendering into internal-RAM strips and copying into PSRAM.

**What costs fps** (profile with `micros()` around sections of the `LV_EVENT_DRAW_MAIN` handlers when tuning). Every dirty area re-runs every overlapping layer's draw handler, so cost is per invalidated pixel *and* per area. Rules that follow from measurements:
- Invalidate with `d2::invalidate()`, not `lv_obj_invalidate_area()`: LVGL 8.4 pads every area by 5 px per side (`lv_obj_get_transformed_area`), which tripled the size of small areas like raindrops.
- `d2::` draw helpers cull against the clip area before calling LVGL; LVGL's own rejection costs ~15 µs per primitive, which adds up across dozens of areas. Keep new primitives going through `d2::`.
- `main.cpp`'s frame timer animates once per completed refresh (`lvgl_port_refresh_count()`); ticking faster than rendering piled up invalidations until LVGL's area list (`LV_INV_BUF_SIZE` 64) overflowed into near-full redraws.
- `pet_view` invalidates only the box it paints (per form/accessory, `paint_areas()`) plus a separate particle box, never its whole object; stars invalidate individually; rain/snow invalidate tight boxes at their old and new positions.
- Rain streaks are plain vertical bars: anti-aliased slanted lines cost ~7× more per area (line masks).
- On PSRAM boards the static backdrop (sky, sun/moon, hills, ground) is pre-rendered into a PSRAM image (`build_bg_cache()` via an offscreen draw context, like `lv_snapshot`), copied row by row into each dirty area, and rebuilt only when the hour or weather changes.
- `LV_CIRCLE_CACHE_SIZE` is 16 because the pet uses many radii.
- `scene_set_clock` redraws only the sun/moon's old and new boxes when the sky colours haven't changed; redrawing the whole backdrop every minute was a visible ~0.25 s hitch on the S3.
- `pet_view_tick` steps animation at ≤ 1/60 s whatever the frame rate: the squash spring is stiff and, stepped once per slow frame, overshoots into jittery bouncing.
- Tested and rejected on the S3: caching the pet's rendered appearance (it changes in ~80% of frames), internal-RAM render buffers (no gain).

**LVGL gotchas hit here:** sizes are computed lazily, so call `lv_obj_update_layout()` before reading a new object's size (`pet_view_create` does). Delete sheets with `lv_obj_del_async` because they are closed from their own click handlers. The built-in Montserrat fonts include `°` and `•` but not `·`. Fonts enabled: Montserrat 12/14/16/18/20/24/28/40/48 (sizes a board doesn't use are stripped by the linker).

**IMU axes** (`hw/imu.cpp` `raw_to_screen`) were measured on the C6 board: screen x = −ax, y = −ay, z = +az (gravity reads z = −1 face up).

**UI** (`ui/`). `home.cpp` owns the home screen (touch: stroke the pet to pat it, tap poop to clean it, swipe up or tap the bottom edge for the menu; tap the clock 5 times quickly for the "Start over?" dialog; reactions to IMU gestures; toasts; friend visits). `sheets.cpp` has the menu drawer, feed shop, play picker, stats, pals list, and the start-over confirm. `games/games.cpp` has Treat Catch (tilt or drag) and Bubble Pop, each on its own screen; `home_show()` fades back and auto-deletes the game screen.
