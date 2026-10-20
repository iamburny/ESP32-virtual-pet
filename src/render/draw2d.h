#pragma once

// Thin helpers over LVGL 8's anti-aliased draw primitives, for use inside
// LV_EVENT_DRAW_MAIN handlers. Coordinates are absolute screen pixels.

#include <lvgl.h>

namespace d2 {

inline void rect(lv_draw_ctx_t *ctx, int x, int y, int w, int h, int radius, lv_color_t c, lv_opa_t opa = LV_OPA_COVER) {
  if (w <= 0 || h <= 0 || opa < LV_OPA_MIN) return;
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.radius = radius;
  d.bg_color = c;
  d.bg_opa = opa;
  lv_area_t a = {(lv_coord_t)x, (lv_coord_t)y, (lv_coord_t)(x + w - 1), (lv_coord_t)(y + h - 1)};
  lv_draw_rect(ctx, &d, &a);
}

// Rounded rect with a vertical two-colour gradient.
inline void grad_rect(lv_draw_ctx_t *ctx, int x, int y, int w, int h, int radius, lv_color_t top, lv_color_t bottom,
                      lv_opa_t opa = LV_OPA_COVER) {
  if (w <= 0 || h <= 0) return;
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.radius = radius;
  d.bg_opa = opa;
  d.bg_color = top;
  d.bg_grad.dir = LV_GRAD_DIR_VER;
  d.bg_grad.stops_count = 2;
  d.bg_grad.stops[0].color = top;
  d.bg_grad.stops[0].frac = 0;
  d.bg_grad.stops[1].color = bottom;
  d.bg_grad.stops[1].frac = 255;
  lv_area_t a = {(lv_coord_t)x, (lv_coord_t)y, (lv_coord_t)(x + w - 1), (lv_coord_t)(y + h - 1)};
  lv_draw_rect(ctx, &d, &a);
}

// Pill / ellipse-ish shape centred on (cx, cy).
inline void oval(lv_draw_ctx_t *ctx, int cx, int cy, int rx, int ry, lv_color_t c, lv_opa_t opa = LV_OPA_COVER) {
  rect(ctx, cx - rx, cy - ry, rx * 2, ry * 2, LV_RADIUS_CIRCLE, c, opa);
}

inline void circle(lv_draw_ctx_t *ctx, int cx, int cy, int r, lv_color_t c, lv_opa_t opa = LV_OPA_COVER) {
  oval(ctx, cx, cy, r, r, c, opa);
}

inline void ring(lv_draw_ctx_t *ctx, int cx, int cy, int r, int width, lv_color_t c, lv_opa_t opa = LV_OPA_COVER) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.radius = LV_RADIUS_CIRCLE;
  d.bg_opa = LV_OPA_TRANSP;
  d.border_color = c;
  d.border_width = width;
  d.border_opa = opa;
  lv_area_t a = {(lv_coord_t)(cx - r), (lv_coord_t)(cy - r), (lv_coord_t)(cx + r - 1), (lv_coord_t)(cy + r - 1)};
  lv_draw_rect(ctx, &d, &a);
}

inline void line(lv_draw_ctx_t *ctx, int x1, int y1, int x2, int y2, int width, lv_color_t c, lv_opa_t opa = LV_OPA_COVER) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.color = c;
  d.width = width;
  d.opa = opa;
  d.round_start = 1;
  d.round_end = 1;
  lv_point_t p1 = {(lv_coord_t)x1, (lv_coord_t)y1};
  lv_point_t p2 = {(lv_coord_t)x2, (lv_coord_t)y2};
  lv_draw_line(ctx, &d, &p1, &p2);
}

// Angles in degrees, 0 = +x (right), increasing clockwise.
inline void arc(lv_draw_ctx_t *ctx, int cx, int cy, int r, int start, int end, int width, lv_color_t c,
                lv_opa_t opa = LV_OPA_COVER) {
  if (r <= 0) return;
  lv_draw_arc_dsc_t d;
  lv_draw_arc_dsc_init(&d);
  d.color = c;
  d.width = width;
  d.opa = opa;
  d.rounded = 1;
  lv_point_t center = {(lv_coord_t)cx, (lv_coord_t)cy};
  lv_draw_arc(ctx, &d, &center, r, (uint16_t)((start % 360 + 360) % 360), (uint16_t)((end % 360 + 360) % 360));
}

inline void poly(lv_draw_ctx_t *ctx, const lv_point_t *pts, int n, lv_color_t c, lv_opa_t opa = LV_OPA_COVER) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = c;
  d.bg_opa = opa;
  lv_draw_polygon(ctx, &d, pts, n);
}

inline void tri(lv_draw_ctx_t *ctx, int x1, int y1, int x2, int y2, int x3, int y3, lv_color_t c, lv_opa_t opa = LV_OPA_COVER) {
  lv_point_t p[3] = {{(lv_coord_t)x1, (lv_coord_t)y1}, {(lv_coord_t)x2, (lv_coord_t)y2}, {(lv_coord_t)x3, (lv_coord_t)y3}};
  poly(ctx, p, 3, c, opa);
}

inline void text(lv_draw_ctx_t *ctx, int x, int y, int w, const char *txt, const lv_font_t *font, lv_color_t c,
                 lv_text_align_t align = LV_TEXT_ALIGN_CENTER, lv_opa_t opa = LV_OPA_COVER) {
  lv_draw_label_dsc_t d;
  lv_draw_label_dsc_init(&d);
  d.color = c;
  d.font = font;
  d.align = align;
  d.opa = opa;
  lv_area_t a = {(lv_coord_t)x, (lv_coord_t)y, (lv_coord_t)(x + w - 1), (lv_coord_t)(y + font->line_height)};
  lv_draw_label(ctx, &d, &a, txt, NULL);
}

inline lv_color_t mix(lv_color_t a, lv_color_t b, float t) {
  if (t <= 0) return a;
  if (t >= 1) return b;
  return lv_color_mix(b, a, (uint8_t)(t * 255));
}

}  // namespace d2
