#include "lvgl_port.h"

#include <lvgl.h>

#include "display.h"
#include "panel.h"

#define TOUCH_ADDR 0x3B
// The controller occasionally reports no touch for a frame mid-press; only
// treat the finger as lifted after this many empty reads in a row.
#define TOUCH_RELEASE_READS 3

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *framebuf[2];
// Frames are sent by a task on core 0 while core 1 renders the next one.
static SemaphoreHandle_t push_start, push_done;
static uint16_t *volatile push_buf;
static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;
static TouchFilter touch_filter;
static bool filtering_press;
static uint32_t stat_frames, stat_ms, stat_px;
static volatile uint32_t refresh_count;

// The AXS15231B only accepts whole frames (sending just the changed rows garbles
// it). LVGL runs in double-buffered direct mode: it redraws only the dirty areas,
// in place, in one of two full-screen PSRAM buffers, and copies them into the other
// buffer before drawing the next frame there. Once the last area of a refresh is
// drawn, the whole buffer goes to the push task on core 0, so sending one frame
// (~22 ms) overlaps rendering the next. LVGL is told the flush is done straight
// away; that's safe because it next draws into the other buffer and only reads
// this one, and the next hand-over waits for this push to finish first.
// LVGL renders byte-swapped (LV_COLOR_16_SWAP) so frames go out as-is.
static void push_task(void *) {
  for (;;) {
    xSemaphoreTake(push_start, portMAX_DELAY);
    panel->draw16bitBeRGBBitmap(0, 0, push_buf, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    xSemaphoreGive(push_done);
  }
}

static void disp_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
  if (lv_disp_flush_is_last(drv)) {
    xSemaphoreTake(push_done, portMAX_DELAY);
    push_buf = (uint16_t *)drv->draw_buf->buf_act;
    xSemaphoreGive(push_start);
  }
  lv_disp_flush_ready(drv);
}


static void monitor_cb(lv_disp_drv_t *drv, uint32_t time_ms, uint32_t px) {
  stat_frames++;
  refresh_count++;
  stat_ms += time_ms;
  stat_px += px;
}

// Reads the AXS15231 directly: an 8-byte read command, then a 14-byte report
// with the touch count in [1] and 12-bit x/y in [2..5].
static bool touch_read(int16_t &x, int16_t &y) {
  static const uint8_t cmd[8] = {0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x08};
  uint8_t buf[14] = {0};
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(cmd, sizeof(cmd));
  if (Wire.endTransmission() != 0) return false;
  int n = Wire.requestFrom(TOUCH_ADDR, 14);
  for (int i = 0; i < n && i < 14; i++) buf[i] = Wire.read();
  if (n < 6 || buf[0] != 0 || buf[1] == 0 || buf[1] > 2) return false;
  x = ((buf[2] & 0x0F) << 8) | buf[3];
  y = ((buf[4] & 0x0F) << 8) | buf[5];
  x = constrain(x, 0, DISPLAY_WIDTH - 1);
  y = constrain(y, 0, DISPLAY_HEIGHT - 1);
  return true;
}

static void touchpad_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  static int16_t last_x, last_y;
  static uint8_t empty_reads = TOUCH_RELEASE_READS;
  int16_t x, y;
  if (touch_read(x, y)) {
    last_x = x;
    last_y = y;
    empty_reads = 0;
  } else if (empty_reads < TOUCH_RELEASE_READS) {
    empty_reads++;
  }
  bool pressed = empty_reads < TOUCH_RELEASE_READS;

  // A press the filter swallows stays hidden from LVGL until the finger lifts.
  if (pressed && !filtering_press && touch_filter && touch_filter()) filtering_press = true;
  if (!pressed) filtering_press = false;

  data->point.x = last_x;
  data->point.y = last_y;
  data->state = pressed && !filtering_press ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void lvgl_port_init() {
  hw_i2c_init();
  lv_init();

  size_t px = (size_t)DISPLAY_WIDTH * DISPLAY_HEIGHT;
  for (auto &fb : framebuf) fb = (lv_color_t *)heap_caps_calloc(px, sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!framebuf[0] || !framebuf[1]) {
    // Nothing can be drawn without it; usually PSRAM isn't enabled for the build.
    for (;;) {
      Serial.println("[display] framebuffer allocation failed - is PSRAM enabled?");
      delay(2000);
    }
  }
  lv_disp_draw_buf_init(&draw_buf, framebuf[0], framebuf[1], px);

  push_start = xSemaphoreCreateBinary();
  push_done = xSemaphoreCreateBinary();
  xSemaphoreGive(push_done);
  xTaskCreatePinnedToCore(push_task, "push", 4096, nullptr, 2, nullptr, 0);

  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = DISPLAY_WIDTH;
  disp_drv.ver_res = DISPLAY_HEIGHT;
  disp_drv.flush_cb = disp_flush_cb;
  disp_drv.monitor_cb = monitor_cb;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.direct_mode = 1;
  lv_disp_drv_register(&disp_drv);

  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touchpad_read_cb;
  lv_indev_drv_register(&indev_drv);
}

void lvgl_port_set_touch_filter(TouchFilter filter) { touch_filter = filter; }

void lvgl_port_take_stats(uint32_t &frames, uint32_t &render_ms, uint32_t &pixels) {
  frames = stat_frames;
  render_ms = stat_ms;
  pixels = stat_px;
  stat_frames = stat_ms = stat_px = 0;
}

uint32_t lvgl_port_refresh_count() { return refresh_count; }
