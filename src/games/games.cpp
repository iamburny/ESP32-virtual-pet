#include <Arduino.h>
#include <math.h>

#include "app.h"
#include "display.h"
#include "hw/imu.h"
#include "power.h"
#include "render/draw2d.h"
#include "render/icons.h"
#include "render/pet_view.h"
#include "ui/style.h"
#include "ui/ui_internal.h"

#define W DISPLAY_WIDTH
#define H DISPLAY_HEIGHT
#define GAME_SECONDS 60
#define COUNTDOWN_S 3
#define MAX_ITEMS 10
#define MAX_POPS 6
#define CATCH_FEET_Y 300

namespace {

enum GameKind : uint8_t { GAME_NONE, GAME_CATCH, GAME_BUBBLE };
enum Phase : uint8_t { PH_COUNTDOWN, PH_PLAY, PH_RESULTS };
enum ItemKind : uint8_t { IT_APPLE, IT_CANDY, IT_STAR, IT_ROCK, IT_BUBBLE, IT_GOLD_BUBBLE };

struct Item {
  bool live;
  uint8_t kind;
  float x, y, vy, r, phase;
};

struct Pop {
  float x, y, r, t;
  bool gold;
};

GameKind kind = GAME_NONE;
Phase phase;
float t, phase_t, spawn_timer;
uint32_t score, combo;
Item items[MAX_ITEMS];
Pop pops[MAX_POPS];
uint32_t rng = 99;

lv_obj_t *scr, *board, *pet, *score_label, *time_label, *count_label, *combo_label;
float pet_x, pet_v;
bool dragging;
int drag_x;

float frand() {
  rng = rng * 1664525u + 1013904223u;
  return (rng >> 8) / 16777216.0f;
}

void invalidate_circle(float x, float y, float r) {
  lv_area_t a = {(lv_coord_t)(x - r - 2), (lv_coord_t)(y - r - 2), (lv_coord_t)(x + r + 2), (lv_coord_t)(y + r + 2)};
  lv_obj_invalidate_area(board, &a);
}

void invalidate_span(float x, float y0, float y1, float r) {
  float top = fminf(y0, y1), bottom = fmaxf(y0, y1);
  lv_area_t a = {(lv_coord_t)(x - r - 4), (lv_coord_t)(top - r - 4), (lv_coord_t)(x + r + 4), (lv_coord_t)(bottom + r + 4)};
  lv_obj_invalidate_area(board, &a);
}

void draw_item(lv_draw_ctx_t *ctx, const Item &it) {
  int s = (int)(it.r * 2);
  switch (it.kind) {
    case IT_APPLE: icon_draw(ctx, ICON_FRUIT, it.x, it.y, s, lv_color_hex(0xFF5A5A)); break;
    case IT_CANDY: icon_draw(ctx, ICON_CANDY, it.x, it.y, s, lv_color_hex(0xFF9AD5)); break;
    case IT_STAR:
      d2::circle(ctx, it.x, it.y, it.r + 3, lv_color_hex(0xFFE066), LV_OPA_30);
      icon_draw(ctx, ICON_CLEAN, it.x, it.y, s, lv_color_hex(0xFFE066));
      break;
    case IT_ROCK:
      d2::circle(ctx, it.x, it.y, it.r, lv_color_hex(0x6E6A7C));
      d2::circle(ctx, it.x - it.r * 0.3f, it.y - it.r * 0.3f, it.r * 0.3f, lv_color_hex(0x8D89A0));
      d2::circle(ctx, it.x + it.r * 0.35f, it.y + it.r * 0.2f, it.r * 0.2f, lv_color_hex(0x4E4A5C));
      break;
    case IT_BUBBLE:
    case IT_GOLD_BUBBLE: {
      bool gold = it.kind == IT_GOLD_BUBBLE;
      lv_color_t c = lv_color_hex(gold ? 0xFFD54F : 0x9FE7FF);
      float x = it.x + sinf(t * 2 + it.phase) * 6;
      d2::circle(ctx, x, it.y, it.r, c, gold ? 110 : 50);
      d2::ring(ctx, x, it.y, it.r, 2, c, 220);
      d2::circle(ctx, x - it.r * 0.35f, it.y - it.r * 0.4f, it.r * 0.22f, lv_color_white(), 200);
      break;
    }
  }
}

void board_draw_cb(lv_event_t *e) {
  lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
  if (kind == GAME_CATCH) {
    d2::grad_rect(ctx, 0, 0, W, CATCH_FEET_Y, 0, lv_color_hex(0x4FA3F7), lv_color_hex(0xFFD6E8));
    d2::grad_rect(ctx, 0, CATCH_FEET_Y - 4, W, H - CATCH_FEET_Y + 4, 0, lv_color_hex(0x7BD389), lv_color_hex(0x3E9E5C));
  } else {
    d2::grad_rect(ctx, 0, 0, W, H, 0, lv_color_hex(0x1A6E9E), lv_color_hex(0x06182B));
    for (int i = 0; i < 5; i++)  // light shafts
      d2::rect(ctx, 10 + i * 36, 0, 12, H, 6, lv_color_white(), 10);
  }
  for (const auto &it : items)
    if (it.live) draw_item(ctx, it);
  for (const auto &p : pops) {
    if (p.t <= 0) continue;
    float k = 1 - p.t / 0.35f;
    d2::ring(ctx, p.x, p.y, p.r * (1 + k * 0.8f), 2, lv_color_hex(p.gold ? 0xFFD54F : 0xBDEFFF),
             (lv_opa_t)(255 * (1 - k)));
  }
}

void update_labels() {
  lv_label_set_text_fmt(score_label, "%lu", (unsigned long)score);
  int left = phase == PH_PLAY ? (int)ceilf(GAME_SECONDS - phase_t) : GAME_SECONDS;
  lv_label_set_text_fmt(time_label, "%d", left < 0 ? 0 : left);
  if (kind == GAME_BUBBLE) {
    if (combo >= 3) {
      lv_label_set_text_fmt(combo_label, "x%lu combo", (unsigned long)combo);
      lv_obj_clear_flag(combo_label, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(combo_label, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void spawn() {
  for (auto &it : items) {
    if (it.live) continue;
    float progress = phase_t / GAME_SECONDS;
    it.live = true;
    it.phase = frand() * 6.28f;
    if (kind == GAME_CATCH) {
      float r = frand();
      it.kind = r < 0.45f ? IT_APPLE : r < 0.7f ? IT_CANDY : r < 0.78f ? IT_STAR : IT_ROCK;
      it.r = 10;
      it.x = 16 + frand() * (W - 32);
      it.y = -12;
      it.vy = 70 + progress * 120 + frand() * 30;
    } else {
      it.kind = frand() < 0.07f ? IT_GOLD_BUBBLE : IT_BUBBLE;
      it.r = 12 + frand() * 12;
      it.x = 24 + frand() * (W - 48);
      it.y = H + it.r;
      it.vy = -(40 + progress * 70 + frand() * 25);
    }
    return;
  }
}

void add_pop(float x, float y, float r, bool gold) {
  for (auto &p : pops) {
    if (p.t <= 0) {
      p = {x, y, r, 0.35f, gold};
      return;
    }
  }
}

void finish() {
  phase = PH_RESULTS;
  uint32_t coins_before = g_pet.coins;
  app_game_over(score);

  lv_obj_t *panel = ui_panel(scr, 148, 150);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_CLICKABLE);  // taps go to the screen, which closes the game
  lv_obj_center(panel);
  lv_obj_t *title = ui_label(panel, "Time's up!", &lv_font_montserrat_16, COL_TEXT);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);
  lv_obj_t *big = ui_label(panel, "", &lv_font_montserrat_48, COL_TEXT);
  lv_label_set_text_fmt(big, "%lu", (unsigned long)score);
  lv_obj_align(big, LV_ALIGN_TOP_MID, 0, 36);
  lv_obj_t *coins = ui_label(panel, "", &lv_font_montserrat_14, COL_COIN);
  lv_label_set_text_fmt(coins, "+%lu coins", (unsigned long)(g_pet.coins - coins_before));
  lv_obj_align(coins, LV_ALIGN_TOP_MID, 0, 94);
  lv_obj_t *hint = ui_label(panel, "tap to continue", &lv_font_montserrat_12, COL_SUBTEXT);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
  pet_view_play(pet, score >= 20 ? ANIM_DANCE : ANIM_HAPPY);
}

void end_game() {
  kind = GAME_NONE;
  home_show();  // fades back and deletes this screen
  scr = nullptr;
}

bool hit_bubble(int px, int py) {
  for (auto &it : items) {
    if (!it.live) continue;
    float x = it.x + sinf(t * 2 + it.phase) * 6;
    float dx = px - x, dy = py - it.y;
    if (dx * dx + dy * dy > (it.r + 8) * (it.r + 8)) continue;
    bool gold = it.kind == IT_GOLD_BUBBLE;
    combo++;
    score += gold ? 5 : 1 + combo / 5;
    add_pop(x, it.y, it.r, gold);
    invalidate_circle(x, it.y, it.r * 2);
    it.live = false;
    if (combo % 5 == 0) pet_view_play(pet, ANIM_HAPPY);
    return true;
  }
  return false;
}

void touch_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_indev_t *indev = lv_indev_get_act();
  if (!indev) return;
  lv_point_t p;
  lv_indev_get_point(indev, &p);

  if (phase == PH_RESULTS) {
    if (code == LV_EVENT_CLICKED && phase_t > 0.8f) end_game();
    return;
  }
  if (phase != PH_PLAY) return;
  if (kind == GAME_CATCH) {
    dragging = code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING;
    drag_x = p.x;
  } else if (code == LV_EVENT_PRESSED) {
    hit_bubble(p.x, p.y);
  }
}

void update_catch(float dt) {
  // Steer by dragging, or by tilting the board.
  float target_v = 0;
  if (dragging)
    target_v = (drag_x - pet_x) * 8;
  else if (imu_state().ok)
    target_v = imu_state().gx * 420;
  pet_v += (target_v - pet_v) * fminf(1, dt * 10);
  pet_x += pet_v * dt;
  if (pet_x < 22) pet_x = 22, pet_v = 0;
  if (pet_x > W - 22) pet_x = W - 22, pet_v = 0;
  lv_obj_set_x(pet, (int)pet_x - lv_obj_get_width(pet) / 2);
  pet_view_set_lean(pet, pet_v / 300);

  float catch_y = CATCH_FEET_Y - 40;
  for (auto &it : items) {
    if (!it.live) continue;
    float oy = it.y;
    it.y += it.vy * dt;
    invalidate_span(it.x, oy, it.y, it.r);
    if (it.y > catch_y && it.y < catch_y + 26 && fabsf(it.x - pet_x) < 30) {
      it.live = false;
      switch (it.kind) {
        case IT_APPLE: score += 1; pet_view_emit_hearts(pet, 1); break;
        case IT_CANDY: score += 2; pet_view_emit_hearts(pet, 1); break;
        case IT_STAR: score += 5; pet_view_emit_sparkles(pet, 6); break;
        case IT_ROCK:
          score = score > 3 ? score - 3 : 0;
          pet_view_play(pet, ANIM_DIZZY);
          break;
      }
      if (it.kind != IT_ROCK && !pet_view_busy(pet)) pet_view_play(pet, ANIM_PAT);
    } else if (it.y > H + 20) {
      it.live = false;
    }
  }
}

void update_bubbles(float dt) {
  for (auto &it : items) {
    if (!it.live) continue;
    float oy = it.y;
    it.y += it.vy * dt;
    invalidate_span(it.x, oy, it.y, it.r + 7);
    if (it.y < -it.r) {
      it.live = false;
      combo = 0;
    }
  }
}

void start(GameKind k) {
  kind = k;
  phase = PH_COUNTDOWN;
  t = phase_t = 0;
  spawn_timer = 0;
  score = combo = 0;
  dragging = false;
  rng ^= millis();
  memset(items, 0, sizeof(items));
  memset(pops, 0, sizeof(pops));

  scr = lv_obj_create(nullptr);
  lv_obj_remove_style_all(scr);
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  board = ui_box(scr);
  lv_obj_set_size(board, W, H);
  lv_obj_add_event_cb(board, board_draw_cb, LV_EVENT_DRAW_MAIN, nullptr);

  int size = k == GAME_CATCH ? 50 : 44;
  pet = pet_view_create(scr, size);
  lv_obj_clear_flag(pet, LV_OBJ_FLAG_CLICKABLE);
  pet_view_set_look(pet, g_pet.form, g_pet.palette, g_pet.stage);
  pet_view_set_mood(pet, MOOD_HAPPY);
  pet_x = W / 2.0f;
  pet_v = 0;
  int feet = k == GAME_CATCH ? CATCH_FEET_Y : H - 6;
  lv_obj_set_pos(pet, (int)pet_x - lv_obj_get_width(pet) / 2, feet + 8 - lv_obj_get_height(pet));

  lv_obj_t *sp = ui_glass(scr, 64, 28);
  lv_obj_set_pos(sp, 6, 6);
  score_label = ui_label(sp, "0", &lv_font_montserrat_20, COL_TEXT);
  lv_obj_center(score_label);
  lv_obj_t *tp = ui_glass(scr, 44, 28);
  lv_obj_set_pos(tp, W - 50, 6);
  time_label = ui_label(tp, "60", &lv_font_montserrat_16, COL_TEXT);
  lv_obj_center(time_label);
  combo_label = ui_label(scr, "", &lv_font_montserrat_14, COL_COIN);
  lv_obj_align(combo_label, LV_ALIGN_TOP_MID, 0, 40);
  lv_obj_add_flag(combo_label, LV_OBJ_FLAG_HIDDEN);

  count_label = ui_label(scr, "3", &lv_font_montserrat_48, COL_TEXT);
  lv_obj_center(count_label);

  lv_obj_t *hint = ui_label(scr, k == GAME_CATCH ? "Tilt or drag to catch treats.\nAvoid the rocks!" : "Pop the bubbles!\nGold ones are worth more",
                            &lv_font_montserrat_12, COL_TEXT);
  lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(hint, LV_ALIGN_CENTER, 0, 50);
  lv_obj_set_user_data(count_label, hint);

  lv_obj_add_event_cb(scr, touch_cb, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(scr, touch_cb, LV_EVENT_PRESSING, nullptr);
  lv_obj_add_event_cb(scr, touch_cb, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(scr, touch_cb, LV_EVENT_CLICKED, nullptr);

  update_labels();
  lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, false);
}

}  // namespace

void game_start_catch() { start(GAME_CATCH); }
void game_start_bubble() { start(GAME_BUBBLE); }
bool game_active() { return kind != GAME_NONE; }

void game_frame(uint32_t dt_ms) {
  if (!scr) return;
  power_activity();  // tilt-only play never touches the screen, so keep it from dimming
  float dt = dt_ms / 1000.0f;
  t += dt;
  phase_t += dt;

  if (phase == PH_COUNTDOWN) {
    int n = COUNTDOWN_S - (int)phase_t;
    lv_label_set_text_fmt(count_label, "%d", n > 0 ? n : 1);
    if (phase_t >= COUNTDOWN_S) {
      lv_obj_del((lv_obj_t *)lv_obj_get_user_data(count_label));
      lv_obj_del(count_label);
      phase = PH_PLAY;
      phase_t = 0;
    }
  } else if (phase == PH_PLAY) {
    float progress = phase_t / GAME_SECONDS;
    spawn_timer -= dt;
    if (spawn_timer <= 0) {
      spawn();
      spawn_timer = kind == GAME_CATCH ? 0.9f - progress * 0.55f : 0.7f - progress * 0.4f;
    }
    if (kind == GAME_CATCH)
      update_catch(dt);
    else
      update_bubbles(dt);
    for (auto &p : pops) {
      if (p.t <= 0) continue;
      p.t -= dt;
      invalidate_circle(p.x, p.y, p.r * 2);
    }
    if (phase_t >= GAME_SECONDS) {
      phase_t = 0;
      finish();
    }
  }
  if (phase != PH_RESULTS || phase_t < 0.1f) update_labels();
  pet_view_tick(pet, dt_ms);
}
