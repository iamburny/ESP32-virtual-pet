#include "lvgl_port.h"

#include <lvgl.h>

#include "display.h"
#include "esp_lcd_touch_axs5106l.h"
#include "hw/board.h"

// The flush is synchronous, so a second buffer would not overlap rendering with
// SPI transfer; one taller buffer means fewer strips per frame instead.
#define DRAW_BUF_LINES 64

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf;
static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;
static TouchFilter touch_filter;
static bool filtering_press;
static uint32_t stat_frames, stat_ms, stat_px;

static void disp_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
  lv_disp_flush_ready(drv);
}

static void monitor_cb(lv_disp_drv_t *drv, uint32_t time_ms, uint32_t px) {
  stat_frames++;
  stat_ms += time_ms;
  stat_px += px;
}

static void touchpad_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  touch_data_t touch_data;
  bsp_touch_read();
  bool pressed = bsp_touch_get_coordinates(&touch_data);

  // A press the filter swallows stays hidden from LVGL until the finger lifts.
  if (pressed && !filtering_press && touch_filter && touch_filter()) filtering_press = true;
  if (!pressed) filtering_press = false;

  if (pressed && !filtering_press) {
    data->point.x = touch_data.coords[0].x;
    data->point.y = touch_data.coords[0].y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void lvgl_port_init() {
  hw_i2c_init();
  bsp_touch_init(&Wire, TOUCH_RST, TOUCH_INT, gfx->getRotation(), gfx->width(), gfx->height());

  lv_init();

  uint32_t buf_size = DISPLAY_WIDTH * DRAW_BUF_LINES;
  disp_draw_buf = (lv_color_t *)heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, buf_size);

  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = gfx->width();
  disp_drv.ver_res = gfx->height();
  disp_drv.flush_cb = disp_flush_cb;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.monitor_cb = monitor_cb;
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
