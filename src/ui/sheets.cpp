#include <Arduino.h>

#include "app.h"
#include "display.h"
#include "render/pet_view.h"
#include "style.h"
#include "ui.h"
#include "ui_internal.h"

#define SHEET_W (DISPLAY_WIDTH - S(8))
#define SHEET_X S(4)

namespace {

lv_obj_t *layer;
lv_obj_t *sheet;

enum MenuItem : uint8_t { MI_FEED, MI_PLAY, MI_CLEAN, MI_LIGHT, MI_HEAL, MI_STATS, MI_PALS, MI_CLOSE };
struct MenuDef {
  Icon icon;
  const char *label;
  uint32_t color;
};
const MenuDef MENU[] = {
    {ICON_FOOD, "Feed", COL_HUNGER}, {ICON_GAME, "Play", COL_ACCENT},   {ICON_CLEAN, "Clean", COL_HYGIENE},
    {ICON_BULB, "Light", COL_ENERGY}, {ICON_PILL, "Heal", COL_WARN},    {ICON_STATS, "Stats", COL_HEALTH},
    {ICON_FRIENDS, "Pals", COL_HAPPY}, {ICON_CLOSE, "Close", COL_SUBTEXT},
};

const Icon FOOD_ICONS[FOOD_COUNT] = {ICON_FOOD, ICON_FRUIT, ICON_CANDY, ICON_CAKE};
const uint32_t FOOD_COLORS[FOOD_COUNT] = {0xE8A15A, 0xFF5A5A, 0xFF9AD5, 0xFFB3C7};

void anim_y_cb(void *obj, int32_t v) { lv_obj_set_y((lv_obj_t *)obj, v); }

lv_obj_t *open_sheet(int h, int y) {
  // Called from click handlers inside the current sheet, so delete it asynchronously.
  sheets_close_all();
  sheet = ui_panel(layer, SHEET_W, h);
  lv_obj_set_pos(sheet, SHEET_X, DISPLAY_HEIGHT);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, sheet);
  lv_anim_set_exec_cb(&a, anim_y_cb);
  lv_anim_set_values(&a, DISPLAY_HEIGHT, y);
  lv_anim_set_time(&a, 220);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
  return sheet;
}

lv_obj_t *sheet_title(lv_obj_t *parent, const char *text) {
  lv_obj_t *l = ui_label(parent, text, FONT_16, COL_TEXT);
  lv_obj_set_pos(l, S(14), S(12));
  return l;
}

void close_cb(lv_event_t *) { sheets_close_all(); }

// ---------------------------------------------------------------- feed

void feed_cb(lv_event_t *e) {
  Food food = (Food)(uintptr_t)lv_event_get_user_data(e);
  sheets_close_all();
  lv_obj_t *pet = home_pet();
  if (g_pet.stage == STAGE_EGG) return;
  if (g_pet.asleep) {
    ui_toast("Shh... sleeping", ICON_MOON, 0xE6ECFF);
    return;
  }
  if (g_pet.coins < FOODS[food].price) {
    ui_toast("Not enough coins - play a game!", ICON_COIN, COL_COIN);
    home_play(ANIM_REFUSE);
    return;
  }
  if (app_feed(food)) {
    pet_view_set_food(pet, food);
    home_play(ANIM_EAT);
    ui_toast(food == FOOD_CAKE ? "Cake!! Best day ever" : "Yum!", FOOD_ICONS[food], FOOD_COLORS[food]);
  }
}

void open_feed() {
  lv_obj_t *s = open_sheet(S(196), DISPLAY_HEIGHT - S(200));
  sheet_title(s, "Feed");
  lv_obj_t *coins = ui_label(s, "", FONT_14, COL_COIN);
  lv_label_set_text_fmt(coins, "%lu", (unsigned long)g_pet.coins);
  lv_obj_align(coins, LV_ALIGN_TOP_RIGHT, -S(14), S(13));
  lv_obj_t *ci = icon_create(s, ICON_COIN, S(13), lv_color_hex(COL_COIN));
  lv_obj_align_to(ci, coins, LV_ALIGN_OUT_LEFT_MID, -S(4), 0);

  for (int i = 0; i < FOOD_COUNT; i++) {
    const FoodInfo &f = FOODS[i];
    lv_obj_t *row = ui_box(s);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(row, SHEET_W - S(16), S(34));
    lv_obj_set_pos(row, S(8), S(40) + i * S(38));
    lv_obj_set_style_radius(row, S(12), 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(COL_PANEL_2), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(COL_PANEL_3), LV_STATE_PRESSED);
    if (g_pet.coins < f.price) lv_obj_set_style_opa(row, 120, 0);
    lv_obj_add_event_cb(row, feed_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);

    lv_obj_t *ic = icon_create(row, FOOD_ICONS[i], S(20), lv_color_hex(FOOD_COLORS[i]));
    lv_obj_align(ic, LV_ALIGN_LEFT_MID, S(8), 0);
    lv_obj_t *name = ui_label(row, f.name, FONT_14, COL_TEXT);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, S(36), 0);
    lv_obj_t *price = ui_label(row, "", FONT_14, COL_COIN);
    lv_label_set_text_fmt(price, "%u", f.price);
    lv_obj_align(price, LV_ALIGN_RIGHT_MID, -S(10), 0);
    lv_obj_t *pc = icon_create(row, ICON_COIN, S(11), lv_color_hex(COL_COIN));
    lv_obj_align_to(pc, price, LV_ALIGN_OUT_LEFT_MID, -S(3), 0);
  }
}

// ---------------------------------------------------------------- play

void game_cb(lv_event_t *e) {
  int which = (int)(uintptr_t)lv_event_get_user_data(e);
  sheets_close_all();
  if (which == 0)
    game_start_catch();
  else
    game_start_bubble();
}

lv_obj_t *game_card(lv_obj_t *parent, int x, Icon icon, uint32_t color, const char *name, int which) {
  lv_obj_t *b = ui_icon_button(parent, S(70), S(86), icon, color, S(30));
  lv_obj_set_pos(b, x, S(42));
  lv_obj_set_style_radius(b, S(18), 0);
  lv_obj_align(lv_obj_get_child(b, 0), LV_ALIGN_TOP_MID, 0, S(14));
  lv_obj_t *l = ui_label(b, name, FONT_14, COL_TEXT);
  lv_obj_align(l, LV_ALIGN_BOTTOM_MID, 0, -S(12));
  lv_obj_add_event_cb(b, game_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)which);
  return b;
}

void open_play() {
  lv_obj_t *s = open_sheet(S(146), DISPLAY_HEIGHT - S(150));
  sheet_title(s, "Play");
  lv_obj_t *hint = ui_label(s, "60s  -  win coins", FONT_12, COL_SUBTEXT);
  lv_obj_align(hint, LV_ALIGN_TOP_RIGHT, -S(14), S(15));
  int gap = S(6), left = (SHEET_W - 2 * S(70) - gap) / 2;
  game_card(s, left, ICON_CATCH, COL_HUNGER, "Catch", 0);
  game_card(s, left + S(70) + gap, ICON_BUBBLE, COL_HYGIENE, "Bubbles", 1);
}

// ---------------------------------------------------------------- stats

void stat_row(lv_obj_t *parent, int y, Icon icon, uint32_t color, const char *name, float value) {
  lv_obj_t *ic = icon_create(parent, icon, S(14), lv_color_hex(color));
  lv_obj_set_pos(ic, S(14), y);
  lv_obj_t *l = ui_label(parent, name, FONT_12, COL_SUBTEXT);
  lv_obj_set_pos(l, S(34), y);
  lv_obj_t *bar = lv_bar_create(parent);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(bar, SHEET_W - S(90), S(8));
  lv_obj_set_pos(bar, S(78), y + S(4));
  lv_bar_set_range(bar, 0, 100);
  lv_bar_set_value(bar, (int)value, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar, lv_color_hex(COL_PANEL_3), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar, lv_color_hex(value < 25 ? COL_WARN : color), LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar, S(4), LV_PART_MAIN);
  lv_obj_set_style_radius(bar, S(4), LV_PART_INDICATOR);
}

void info_row(lv_obj_t *parent, int y, const char *key, const char *value) {
  lv_obj_t *k = ui_label(parent, key, FONT_12, COL_SUBTEXT);
  lv_obj_set_pos(k, S(14), y);
  lv_obj_t *v = ui_label(parent, value, FONT_12, COL_TEXT);
  lv_obj_align(v, LV_ALIGN_TOP_RIGHT, -S(14), y);
}

void open_stats() {
  lv_obj_t *s = open_sheet(S(292), S(14));
  lv_obj_add_event_cb(s, close_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *name = ui_label(s, g_pet.name, FONT_20, COL_TEXT);
  lv_obj_set_pos(name, S(14), S(12));
  lv_obj_t *sub = ui_label(s, "", FONT_12, COL_SUBTEXT);
  lv_label_set_text_fmt(sub, "%s %s  \xE2\x80\xA2  Gen %u", pet_stage_name(g_pet.stage), pet_form_name(g_pet.form),
                        g_pet.generation + 1);
  lv_obj_set_pos(sub, S(14), S(38));

  int y = S(64);
  stat_row(s, y, ICON_FOOD, COL_HUNGER, "Food", g_pet.hunger);
  stat_row(s, y += S(22), ICON_HEART, COL_HAPPY, "Joy", g_pet.happiness);
  stat_row(s, y += S(22), ICON_BOLT, COL_ENERGY, "Energy", g_pet.energy);
  stat_row(s, y += S(22), ICON_DROP, COL_HYGIENE, "Clean", g_pet.hygiene);
  stat_row(s, y += S(22), ICON_PILL, COL_HEALTH, "Health", g_pet.health);

  char buf[24];
  y += S(30);
  uint32_t age = g_pet.age_s;
  snprintf(buf, sizeof(buf), "%lud %luh %lum", (unsigned long)(age / 86400), (unsigned long)(age / 3600 % 24),
           (unsigned long)(age / 60 % 60));
  info_row(s, y, "Age", buf);
  snprintf(buf, sizeof(buf), "%.0f g", g_pet.weight * 10);
  info_row(s, y += S(20), "Weight", buf);
  snprintf(buf, sizeof(buf), "%u", g_pet.care_mistakes);
  info_row(s, y += S(20), "Care mistakes", buf);
  snprintf(buf, sizeof(buf), "%d", g_friend_count);
  info_row(s, y += S(20), "Pals", buf);
  info_row(s, y += S(20), "Status", g_pet.sick ? "Sick" : g_pet.asleep ? "Asleep" : "Awake");

  lv_obj_t *hint = ui_label(s, "tap to close", FONT_12, COL_SUBTEXT);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -S(10));
}

// ---------------------------------------------------------------- pals

void open_pals() {
  lv_obj_t *s = open_sheet(S(236), DISPLAY_HEIGHT - S(240));
  lv_obj_add_event_cb(s, close_cb, LV_EVENT_CLICKED, nullptr);
  sheet_title(s, "Pals");

  if (g_friend_count == 0) {
    lv_obj_t *l = ui_label(s, "No pals yet.\n\nBring another Tomo nearby and they'll come to visit!",
                           FONT_12, COL_SUBTEXT);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, SHEET_W - S(28));
    lv_obj_set_pos(l, S(14), S(44));
    return;
  }
  for (int i = 0; i < g_friend_count && i < 7; i++) {
    const FriendRecord &f = g_friends[i];
    int y = S(42) + i * S(26);
    lv_obj_t *dot = ui_box(s);
    lv_obj_set_size(dot, S(14), S(14));
    lv_obj_set_pos(dot, S(14), y + S(2));
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, pet_palette_main(f.palette), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_t *n = ui_label(s, f.name, FONT_14, COL_TEXT);
    lv_obj_set_pos(n, S(36), y);
    lv_obj_t *v = ui_label(s, "", FONT_12, COL_SUBTEXT);
    lv_label_set_text_fmt(v, "%s  x%u", pet_form_name(f.form), f.visits);
    lv_obj_align(v, LV_ALIGN_TOP_RIGHT, -S(14), y + S(2));
  }
}

// ---------------------------------------------------------------- menu

void menu_cb(lv_event_t *e) {
  MenuItem item = (MenuItem)(uintptr_t)lv_event_get_user_data(e);
  bool egg = g_pet.stage == STAGE_EGG;
  switch (item) {
    case MI_FEED:
      if (egg) break;
      open_feed();
      return;
    case MI_PLAY:
      if (egg) break;
      if (g_pet.asleep) {
        sheets_close_all();
        ui_toast("Shh... sleeping", ICON_MOON, 0xE6ECFF);
        return;
      }
      if (g_pet.energy < 15) {
        sheets_close_all();
        ui_toast("Too tired to play", ICON_BOLT, COL_ENERGY);
        home_play(ANIM_REFUSE);
        return;
      }
      open_play();
      return;
    case MI_CLEAN:
      sheets_close_all();
      if (app_clean()) {
        pet_view_emit_sparkles(home_pet(), 8);
        ui_toast("Squeaky clean!", ICON_CLEAN, COL_HYGIENE);
      } else if (!egg) {
        ui_toast("Already spotless", ICON_CLEAN, COL_HYGIENE);
      }
      return;
    case MI_LIGHT:
      sheets_close_all();
      app_toggle_light();
      ui_toast(g_pet.light_on ? "Lights on" : "Lights off", ICON_BULB, COL_ENERGY);
      return;
    case MI_HEAL: {
      sheets_close_all();
      if (egg) return;
      bool was_sick = g_pet.sick;
      app_medicine();
      if (was_sick) {
        home_play(ANIM_MEDICINE);
        ui_toast("All better!", ICON_PILL, COL_HEALTH);
      } else {
        home_play(ANIM_REFUSE);
        ui_toast("Yuck! I'm not sick", ICON_PILL, COL_WARN);
      }
      return;
    }
    case MI_STATS: open_stats(); return;
    case MI_PALS: open_pals(); return;
    default: break;
  }
  sheets_close_all();
  if (egg && item != MI_CLOSE) ui_toast("Wait for it to hatch!", ICON_HEART, COL_HAPPY);
}

}  // namespace

void sheets_init(lv_obj_t *l) { layer = l; }

void sheets_open_menu() {
  lv_obj_t *s = open_sheet(S(140), DISPLAY_HEIGHT - S(144));
  int cell = (SHEET_W - S(12)) / 4, btn = S(34);
  for (int i = 0; i < 8; i++) {
    int col = i % 4, row = i / 4;
    int x = S(6) + col * cell;
    lv_obj_t *b = ui_icon_button(s, btn, btn, MENU[i].icon, MENU[i].color, S(20));
    lv_obj_set_pos(b, x + (cell - btn) / 2, S(10) + row * S(64));
    lv_obj_add_event_cb(b, menu_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    lv_obj_t *l = ui_label(s, MENU[i].label, FONT_12, COL_SUBTEXT);
    lv_obj_set_width(l, cell);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(l, x, S(47) + row * S(64));
  }
}

void sheets_close_all() {
  if (!sheet) return;
  lv_anim_del(sheet, anim_y_cb);
  lv_obj_del_async(sheet);
  sheet = nullptr;
}

bool sheets_any_open() { return sheet != nullptr; }

void sheets_frame(uint32_t) {}
