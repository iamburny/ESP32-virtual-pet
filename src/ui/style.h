#pragma once

#include <lvgl.h>

#include "layout.h"
#include "render/icons.h"

#define COL_PANEL 0x1C1B2E
#define COL_PANEL_2 0x2C2B45
#define COL_PANEL_3 0x3A3858
#define COL_TEXT 0xFFFFFF
#define COL_SUBTEXT 0xA9A6C8
#define COL_ACCENT 0x8B6CFF

#define COL_HUNGER 0xFFA94D
#define COL_HAPPY 0xFF5C8A
#define COL_ENERGY 0xFFD23F
#define COL_HYGIENE 0x4FC3F7
#define COL_HEALTH 0x6EE7A0
#define COL_COIN 0xFFC93D
#define COL_WARN 0xFF5A5A

// Plain container with no theme styling.
inline lv_obj_t *ui_box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_clear_flag(o, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  return o;
}

// Rounded translucent "glass" pill used behind HUD elements.
inline lv_obj_t *ui_glass(lv_obj_t *parent, int w, int h) {
  lv_obj_t *o = ui_box(parent);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(0x0B0A1A), 0);
  lv_obj_set_style_bg_opa(o, 90, 0);
  return o;
}

inline lv_obj_t *ui_panel(lv_obj_t *parent, int w, int h) {
  lv_obj_t *o = ui_box(parent);
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);  // swallow taps so they don't reach the scene
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_radius(o, S(22), 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(COL_PANEL), 0);
  lv_obj_set_style_bg_opa(o, 240, 0);
  lv_obj_set_style_border_color(o, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_opa(o, 25, 0);
  lv_obj_set_style_border_width(o, 1, 0);
  return o;
}

inline lv_obj_t *ui_label(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  lv_label_set_text(l, text);
  return l;
}

// Rounded button with a coloured icon; pressing shrinks it slightly.
inline lv_obj_t *ui_icon_button(lv_obj_t *parent, int w, int h, Icon icon, uint32_t icon_color, int icon_size) {
  lv_obj_t *b = ui_box(parent);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_radius(b, S(14), 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(COL_PANEL_2), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(COL_PANEL_3), LV_STATE_PRESSED);
  lv_obj_set_style_transform_width(b, -S(2), LV_STATE_PRESSED);
  lv_obj_set_style_transform_height(b, -S(2), LV_STATE_PRESSED);
  lv_obj_t *ic = icon_create(b, icon, icon_size, lv_color_hex(icon_color));
  lv_obj_center(ic);
  return b;
}
