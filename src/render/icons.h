#pragma once

#include <lvgl.h>

enum Icon : uint8_t {
  ICON_FOOD,
  ICON_HEART,
  ICON_BOLT,
  ICON_DROP,
  ICON_GAME,
  ICON_CLEAN,
  ICON_BULB,
  ICON_PILL,
  ICON_STATS,
  ICON_COIN,
  ICON_FRIENDS,
  ICON_CLOSE,
  ICON_SUN,
  ICON_MOON,
  ICON_CLOUD,
  ICON_RAIN,
  ICON_SNOW,
  ICON_STORM,
  ICON_FOG,
  ICON_CATCH,
  ICON_BUBBLE,
  ICON_CAKE,
  ICON_CANDY,
  ICON_FRUIT,
};

// Draws `icon` centred on (cx, cy) fitting a box of roughly size × size.
void icon_draw(lv_draw_ctx_t *ctx, Icon icon, int cx, int cy, int size, lv_color_t color);

// An LVGL object that just draws one icon centred in itself.
lv_obj_t *icon_create(lv_obj_t *parent, Icon icon, int size, lv_color_t color);
void icon_set(lv_obj_t *obj, Icon icon, lv_color_t color);
