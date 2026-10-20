#include "icons.h"

#include <math.h>

#include "draw2d.h"

// Icons are designed on a 16×16 grid centred on (8, 8) and scaled to `size`.
struct Grid {
  int cx, cy;
  float k;
  int x(float v) const { return cx + (int)lroundf((v - 8) * k); }
  int y(float v) const { return cy + (int)lroundf((v - 8) * k); }
  int l(float v) const {
    int r = (int)lroundf(v * k);
    return r < 1 ? 1 : r;
  }
};

static void cloud(lv_draw_ctx_t *ctx, const Grid &g, float dy, lv_color_t c) {
  d2::circle(ctx, g.x(5.5f), g.y(8.5f + dy), g.l(3.2f), c);
  d2::circle(ctx, g.x(9.5f), g.y(7 + dy), g.l(4.2f), c);
  d2::circle(ctx, g.x(12.5f), g.y(9.5f + dy), g.l(2.8f), c);
  d2::rect(ctx, g.x(2.5f), g.y(8.8f + dy), g.l(12.5f), g.l(3.6f), g.l(1.8f), c);
}

void icon_draw(lv_draw_ctx_t *ctx, Icon icon, int cx, int cy, int size, lv_color_t c) {
  Grid g = {cx, cy, size / 16.0f};
  lv_color_t dark = lv_color_black();

  switch (icon) {
    case ICON_FOOD:
      d2::line(ctx, g.x(7), g.y(9), g.x(12.5f), g.y(14.5f), g.l(2.4f), c);
      d2::circle(ctx, g.x(13.5f), g.y(13), g.l(1.5f), c);
      d2::circle(ctx, g.x(11.5f), g.y(15), g.l(1.5f), c);
      d2::oval(ctx, g.x(6), g.y(6), g.l(5), g.l(4.6f), c);
      d2::circle(ctx, g.x(4.5f), g.y(4.5f), g.l(1.3f), lv_color_white(), LV_OPA_50);
      break;
    case ICON_FRUIT:
      d2::circle(ctx, g.x(6), g.y(9.5f), g.l(4.5f), c);
      d2::circle(ctx, g.x(10), g.y(9.5f), g.l(4.5f), c);
      d2::line(ctx, g.x(8), g.y(5), g.x(9), g.y(2), g.l(1.4f), c);
      d2::line(ctx, g.x(9.5f), g.y(3.5f), g.x(12.5f), g.y(2.5f), g.l(2), c);
      break;
    case ICON_HEART:
      d2::circle(ctx, g.x(5.2f), g.y(6.2f), g.l(3.6f), c);
      d2::circle(ctx, g.x(10.8f), g.y(6.2f), g.l(3.6f), c);
      d2::tri(ctx, g.x(1.7f), g.y(7.2f), g.x(14.3f), g.y(7.2f), g.x(8), g.y(14.5f), c);
      break;
    case ICON_BOLT:
      d2::tri(ctx, g.x(10.5f), g.y(0.5f), g.x(3), g.y(9.5f), g.x(9.2f), g.y(9.5f), c);
      d2::tri(ctx, g.x(6.8f), g.y(6.5f), g.x(13), g.y(6.5f), g.x(5.5f), g.y(15.5f), c);
      break;
    case ICON_DROP:
      d2::circle(ctx, g.x(8), g.y(10.2f), g.l(4.8f), c);
      d2::tri(ctx, g.x(8), g.y(0.8f), g.x(3.5f), g.y(9), g.x(12.5f), g.y(9), c);
      d2::circle(ctx, g.x(6.3f), g.y(10.5f), g.l(1.3f), lv_color_white(), LV_OPA_60);
      break;
    case ICON_GAME:
      d2::rect(ctx, g.x(1), g.y(4.5f), g.l(14), g.l(8), g.l(3.5f), c);
      d2::line(ctx, g.x(3.5f), g.y(8.5f), g.x(6.5f), g.y(8.5f), g.l(1.3f), dark, LV_OPA_60);
      d2::line(ctx, g.x(5), g.y(7), g.x(5), g.y(10), g.l(1.3f), dark, LV_OPA_60);
      d2::circle(ctx, g.x(10.5f), g.y(9.2f), g.l(1.1f), dark, LV_OPA_60);
      d2::circle(ctx, g.x(12.3f), g.y(7.4f), g.l(1.1f), dark, LV_OPA_60);
      break;
    case ICON_CLEAN: {
      lv_point_t v[4] = {{(lv_coord_t)g.x(7), (lv_coord_t)g.y(1.5f)},
                         {(lv_coord_t)g.x(8.8f), (lv_coord_t)g.y(8.5f)},
                         {(lv_coord_t)g.x(7), (lv_coord_t)g.y(15.5f)},
                         {(lv_coord_t)g.x(5.2f), (lv_coord_t)g.y(8.5f)}};
      lv_point_t h[4] = {{(lv_coord_t)g.x(0.5f), (lv_coord_t)g.y(8.5f)},
                         {(lv_coord_t)g.x(7), (lv_coord_t)g.y(6.7f)},
                         {(lv_coord_t)g.x(13.5f), (lv_coord_t)g.y(8.5f)},
                         {(lv_coord_t)g.x(7), (lv_coord_t)g.y(10.3f)}};
      d2::poly(ctx, v, 4, c);
      d2::poly(ctx, h, 4, c);
      d2::circle(ctx, g.x(13), g.y(3), g.l(1.8f), c);
      d2::circle(ctx, g.x(13.5f), g.y(13.5f), g.l(1.2f), c);
      break;
    }
    case ICON_BULB:
      d2::circle(ctx, g.x(8), g.y(6.5f), g.l(5.2f), c);
      d2::rect(ctx, g.x(5.5f), g.y(9.5f), g.l(5), g.l(3.5f), g.l(1), c);
      d2::line(ctx, g.x(6.2f), g.y(14.5f), g.x(9.8f), g.y(14.5f), g.l(1.6f), c);
      d2::circle(ctx, g.x(6.3f), g.y(5), g.l(1.4f), lv_color_white(), LV_OPA_60);
      break;
    case ICON_PILL:
      d2::line(ctx, g.x(4), g.y(12), g.x(12), g.y(4), g.l(6.5f), c);
      d2::line(ctx, g.x(4), g.y(12), g.x(8), g.y(8), g.l(6.5f), dark, LV_OPA_30);
      break;
    case ICON_STATS:
      d2::rect(ctx, g.x(1.5f), g.y(9), g.l(3.5f), g.l(6), g.l(1.2f), c);
      d2::rect(ctx, g.x(6.25f), g.y(5), g.l(3.5f), g.l(10), g.l(1.2f), c);
      d2::rect(ctx, g.x(11), g.y(1.5f), g.l(3.5f), g.l(13.5f), g.l(1.2f), c);
      break;
    case ICON_COIN:
      d2::circle(ctx, g.x(8), g.y(8), g.l(7.2f), c);
      d2::ring(ctx, g.x(8), g.y(8), g.l(5.2f), g.l(1.2f), dark, LV_OPA_20);
      d2::line(ctx, g.x(8), g.y(5), g.x(8), g.y(11), g.l(1.8f), dark, LV_OPA_30);
      break;
    case ICON_FRIENDS:
      d2::circle(ctx, g.x(11), g.y(5.5f), g.l(2.6f), c, LV_OPA_70);
      d2::oval(ctx, g.x(11.5f), g.y(12.5f), g.l(4), g.l(3), c, LV_OPA_70);
      d2::circle(ctx, g.x(5.5f), g.y(5), g.l(3), c);
      d2::oval(ctx, g.x(5.5f), g.y(12.2f), g.l(4.8f), g.l(3.6f), c);
      break;
    case ICON_CLOSE:
      d2::line(ctx, g.x(3), g.y(3), g.x(13), g.y(13), g.l(2.4f), c);
      d2::line(ctx, g.x(13), g.y(3), g.x(3), g.y(13), g.l(2.4f), c);
      break;
    case ICON_SUN:
      d2::circle(ctx, g.x(8), g.y(8), g.l(4), c);
      for (int i = 0; i < 8; i++) {
        float a = i * (float)M_PI / 4;
        d2::line(ctx, g.x(8 + cosf(a) * 6), g.y(8 + sinf(a) * 6), g.x(8 + cosf(a) * 7.6f), g.y(8 + sinf(a) * 7.6f),
                 g.l(1.6f), c);
      }
      break;
    case ICON_MOON:
      d2::arc(ctx, g.x(8), g.y(8), g.l(6.5f), 70, 290, g.l(4), c);
      break;
    case ICON_CLOUD:
      cloud(ctx, g, 0.5f, c);
      break;
    case ICON_RAIN:
      cloud(ctx, g, -2.5f, c);
      for (int i = 0; i < 3; i++) d2::line(ctx, g.x(5 + i * 3.2f), g.y(12), g.x(4 + i * 3.2f), g.y(15), g.l(1.4f), c);
      break;
    case ICON_SNOW:
      cloud(ctx, g, -2.5f, c);
      for (int i = 0; i < 3; i++) d2::circle(ctx, g.x(4.8f + i * 3.2f), g.y(13.5f + (i & 1)), g.l(1.2f), c);
      break;
    case ICON_STORM:
      cloud(ctx, g, -2.5f, c);
      d2::tri(ctx, g.x(9), g.y(10), g.x(5.5f), g.y(13.5f), g.x(8), g.y(13.5f), c);
      d2::tri(ctx, g.x(7.2f), g.y(12.5f), g.x(10.5f), g.y(12.5f), g.x(6.5f), g.y(16), c);
      break;
    case ICON_FOG:
      d2::line(ctx, g.x(2), g.y(4.5f), g.x(12), g.y(4.5f), g.l(1.8f), c);
      d2::line(ctx, g.x(4), g.y(8), g.x(14), g.y(8), g.l(1.8f), c);
      d2::line(ctx, g.x(2), g.y(11.5f), g.x(11), g.y(11.5f), g.l(1.8f), c);
      break;
    case ICON_CATCH: {
      lv_point_t b[4] = {{(lv_coord_t)g.x(1.5f), (lv_coord_t)g.y(8.5f)},
                         {(lv_coord_t)g.x(14.5f), (lv_coord_t)g.y(8.5f)},
                         {(lv_coord_t)g.x(12.2f), (lv_coord_t)g.y(15)},
                         {(lv_coord_t)g.x(3.8f), (lv_coord_t)g.y(15)}};
      d2::poly(ctx, b, 4, c);
      d2::circle(ctx, g.x(8), g.y(3.5f), g.l(2.8f), c);
      break;
    }
    case ICON_BUBBLE:
      d2::ring(ctx, g.x(7), g.y(9), g.l(6.2f), g.l(1.5f), c);
      d2::circle(ctx, g.x(5), g.y(7), g.l(1.5f), c);
      d2::ring(ctx, g.x(13), g.y(3), g.l(2.4f), g.l(1.1f), c);
      break;
    case ICON_CAKE:
      d2::rect(ctx, g.x(2), g.y(8), g.l(12), g.l(7), g.l(2), c);
      d2::rect(ctx, g.x(2), g.y(8), g.l(12), g.l(2.5f), g.l(1.2f), lv_color_white(), LV_OPA_60);
      d2::line(ctx, g.x(8), g.y(3.8f), g.x(8), g.y(7), g.l(1.6f), c);
      d2::circle(ctx, g.x(8), g.y(2.2f), g.l(1.4f), c);
      break;
    case ICON_CANDY:
      d2::tri(ctx, g.x(1), g.y(4.5f), g.x(1), g.y(11.5f), g.x(6), g.y(8), c);
      d2::tri(ctx, g.x(15), g.y(4.5f), g.x(15), g.y(11.5f), g.x(10), g.y(8), c);
      d2::circle(ctx, g.x(8), g.y(8), g.l(4), c);
      d2::arc(ctx, g.x(8), g.y(8), g.l(2.5f), 200, 290, g.l(1), lv_color_white(), LV_OPA_70);
      break;
  }
}

struct IconData {
  Icon icon;
  lv_color_t color;
};

static void icon_draw_cb(lv_event_t *e) {
  lv_obj_t *obj = lv_event_get_target(e);
  IconData *d = (IconData *)lv_obj_get_user_data(obj);
  if (!d) return;
  lv_area_t a;
  lv_obj_get_coords(obj, &a);
  int size = LV_MIN(lv_area_get_width(&a), lv_area_get_height(&a));
  icon_draw(lv_event_get_draw_ctx(e), d->icon, (a.x1 + a.x2 + 1) / 2, (a.y1 + a.y2 + 1) / 2, size, d->color);
}

static void icon_delete_cb(lv_event_t *e) {
  lv_obj_t *obj = lv_event_get_target(e);
  delete (IconData *)lv_obj_get_user_data(obj);
  lv_obj_set_user_data(obj, nullptr);
}

lv_obj_t *icon_create(lv_obj_t *parent, Icon icon, int size, lv_color_t color) {
  lv_obj_t *obj = lv_obj_create(parent);
  lv_obj_remove_style_all(obj);
  lv_obj_set_size(obj, size, size);
  lv_obj_clear_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_user_data(obj, new IconData{icon, color});
  lv_obj_add_event_cb(obj, icon_draw_cb, LV_EVENT_DRAW_MAIN, nullptr);
  lv_obj_add_event_cb(obj, icon_delete_cb, LV_EVENT_DELETE, nullptr);
  return obj;
}

void icon_set(lv_obj_t *obj, Icon icon, lv_color_t color) {
  IconData *d = (IconData *)lv_obj_get_user_data(obj);
  if (!d || (d->icon == icon && d->color.full == color.full)) return;
  d->icon = icon;
  d->color = color;
  lv_obj_invalidate(obj);
}
