#include <Arduino.h>
#include <time.h>

#include "app.h"
#include "display.h"
#include "hw/imu.h"
#include "net/net_task.h"
#include "power.h"
#include "render/scene.h"
#include "style.h"
#include "ui.h"
#include "ui_internal.h"

#define PET_SIZE S(74)
#define FRIEND_SIZE S(56)
#define TOAST_MS 2800

namespace {

enum VisitPhase : uint8_t { VISIT_NONE, VISIT_ENTER, VISIT_PARTY, VISIT_LEAVE };

lv_obj_t *scr, *pet, *friend_pet, *top_layer;
lv_obj_t *clock_label, *coin_label, *temp_label, *weather_icon, *name_label;
lv_obj_t *rings[4];
lv_obj_t *toast, *toast_icon, *toast_label;
lv_timer_t *toast_timer;

float pet_offset, pet_vel;
int pet_base_x;
uint32_t hud_timer;
char last_name[PET_NAME_LEN];

// Touch tracking.
lv_point_t press_pt, last_pt;
uint32_t press_ms, last_pat_ms;
bool press_on_pet, press_moved, press_swallowed;
float stroke_acc;

// Friend visit.
VisitPhase visit;
float visit_t;
FriendSeen visitor;

const uint32_t RING_COLORS[4] = {COL_HUNGER, COL_HAPPY, COL_ENERGY, COL_HYGIENE};
const Icon RING_ICONS[4] = {ICON_FOOD, ICON_HEART, ICON_BOLT, ICON_DROP};

lv_obj_t *make_ring(lv_obj_t *parent, int i) {
  lv_obj_t *arc = lv_arc_create(parent);
  lv_obj_set_size(arc, S(30), S(30));
  lv_arc_set_rotation(arc, 270);
  lv_arc_set_bg_angles(arc, 0, 360);
  lv_arc_set_range(arc, 0, 100);
  lv_obj_remove_style(arc, nullptr, LV_PART_KNOB);
  lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(arc, S(3), LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_arc_opa(arc, 50, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, S(3), LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc, lv_color_hex(RING_COLORS[i]), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(arc, 90, LV_PART_MAIN);
  lv_obj_set_style_bg_color(arc, lv_color_hex(0x0B0A1A), LV_PART_MAIN);
  lv_obj_set_style_radius(arc, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_pad_all(arc, 0, LV_PART_MAIN);
  lv_obj_t *ic = icon_create(arc, RING_ICONS[i], S(13), lv_color_hex(RING_COLORS[i]));
  lv_obj_center(ic);
  return arc;
}

void build_hud() {
  // Clock on the left, coins centred, weather on the right.
  lv_obj_t *clock_pill = ui_glass(scr, S(64), S(26));
  lv_obj_set_pos(clock_pill, S(6), S(6));
  clock_label = ui_label(clock_pill, "--:--", FONT_20, COL_TEXT);
  lv_obj_center(clock_label);

  lv_obj_t *coin_pill = ui_glass(scr, S(46), S(26));
  int clock_right = S(6) + S(64), wx_left = DISPLAY_WIDTH - S(4) - S(44);
  lv_obj_align(coin_pill, LV_ALIGN_TOP_MID, (clock_right + wx_left) / 2 - DISPLAY_WIDTH / 2, S(6));
  lv_obj_t *ci = icon_create(coin_pill, ICON_COIN, S(14), lv_color_hex(COL_COIN));
  lv_obj_align(ci, LV_ALIGN_LEFT_MID, S(6), 0);
  coin_label = ui_label(coin_pill, "0", FONT_14, COL_TEXT);
  lv_obj_align(coin_label, LV_ALIGN_LEFT_MID, S(23), 0);

  lv_obj_t *wx_pill = ui_glass(scr, S(44), S(26));
  lv_obj_align(wx_pill, LV_ALIGN_TOP_RIGHT, -S(4), S(6));
  weather_icon = icon_create(wx_pill, ICON_SUN, S(16), lv_color_hex(0xFFE066));
  lv_obj_align(weather_icon, LV_ALIGN_LEFT_MID, S(5), 0);
  temp_label = ui_label(wx_pill, "--", FONT_12, COL_TEXT);
  lv_obj_align(temp_label, LV_ALIGN_LEFT_MID, S(23), 0);

  // Stat rings spread evenly across the width.
  for (int i = 0; i < 4; i++) {
    rings[i] = make_ring(scr, i);
    lv_obj_set_pos(rings[i], SX(10 + i * 40) + (SX(30) - S(30)) / 2, S(38));
  }

  name_label = ui_label(scr, "", FONT_12, COL_TEXT);
  lv_obj_set_style_text_opa(name_label, 220, 0);
  lv_obj_align(name_label, LV_ALIGN_TOP_MID, 0, S(74));

  // Swipe-up handle.
  lv_obj_t *handle = ui_box(scr);
  lv_obj_set_size(handle, S(44), S(5));
  lv_obj_set_style_radius(handle, S(3), 0);
  lv_obj_set_style_bg_color(handle, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(handle, 140, 0);
  lv_obj_align(handle, LV_ALIGN_BOTTOM_MID, 0, -S(8));

  int toast_w = DISPLAY_WIDTH - S(12);
  toast = ui_glass(scr, toast_w, S(30));
  lv_obj_set_style_bg_opa(toast, 200, 0);
  lv_obj_align(toast, LV_ALIGN_TOP_MID, 0, S(70));
  toast_icon = icon_create(toast, ICON_HEART, S(16), lv_color_white());
  lv_obj_align(toast_icon, LV_ALIGN_LEFT_MID, S(9), 0);
  toast_label = ui_label(toast, "", FONT_12, COL_TEXT);
  lv_label_set_long_mode(toast_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_width(toast_label, toast_w - S(36));
  lv_obj_align(toast_label, LV_ALIGN_LEFT_MID, S(30), 0);
  lv_obj_add_flag(toast, LV_OBJ_FLAG_HIDDEN);
}

void refresh_hud() {
  time_t now = time(nullptr);
  struct tm tm;
  localtime_r(&now, &tm);
  if (net_time_valid())
    lv_label_set_text_fmt(clock_label, "%02d:%02d", tm.tm_hour, tm.tm_min);
  lv_label_set_text_fmt(coin_label, "%lu", (unsigned long)g_pet.coins);

  NetState ns = net_get();
  if (ns.weather_valid) {
    static const Icon wx_icons[] = {ICON_SUN, ICON_SUN, ICON_CLOUD, ICON_CLOUD, ICON_FOG, ICON_RAIN, ICON_RAIN, ICON_SNOW, ICON_STORM};
    Icon ic = wx_icons[ns.weather <= WX_STORM ? ns.weather : 0];
    if (ic == ICON_SUN && !ns.is_day) ic = ICON_MOON;
    icon_set(weather_icon, ic, lv_color_hex(ic == ICON_SUN ? 0xFFE066 : 0xE6ECFF));
    lv_label_set_text_fmt(temp_label, "%d°", (int)lroundf(ns.temp_c));
  }

  float vals[4] = {g_pet.hunger, g_pet.happiness, g_pet.energy, g_pet.hygiene};
  bool blink = (millis() / 500) & 1;
  for (int i = 0; i < 4; i++) {
    lv_arc_set_value(rings[i], g_pet.stage == STAGE_EGG ? 100 : (int)vals[i]);
    bool low = g_pet.stage != STAGE_EGG && vals[i] < 25;
    lv_obj_set_style_arc_color(rings[i], lv_color_hex(low && blink ? COL_WARN : RING_COLORS[i]), LV_PART_INDICATOR);
  }

  if (g_pet.stage == STAGE_EGG)
    lv_label_set_text(name_label, "An egg... tap it!");
  else
    lv_label_set_text_fmt(name_label, "%s  \xE2\x80\xA2  %s %s", g_pet.name, pet_stage_name(g_pet.stage), pet_form_name(g_pet.form));
}

uint8_t pick_accessories() {
  NetState ns = net_get();
  if (!ns.weather_valid || g_pet.stage == STAGE_EGG) return 0;
  uint8_t acc = 0;
  bool wet = ns.weather == WX_RAIN || ns.weather == WX_DRIZZLE || ns.weather == WX_STORM;
  if (wet && !g_pet.asleep) acc |= ACC_UMBRELLA;
  if (ns.weather == WX_SNOW || ns.temp_c < 3) acc |= ACC_SCARF;
  if (ns.weather == WX_CLEAR && ns.is_day && ns.temp_c >= 22) acc |= ACC_SUNGLASSES;
  return acc;
}

void sync_pet_look() {
  pet_view_set_look(pet, g_pet.form, g_pet.palette, g_pet.stage);
  pet_view_set_mood(pet, pet_mood(g_pet));
  pet_view_set_accessories(pet, pick_accessories());
  if (g_pet.stage == STAGE_EGG) pet_view_set_egg_progress(pet, g_pet.stage_s / 300.0f);
}

void sync_scene() {
  if (net_time_valid()) {
    time_t now = time(nullptr);
    struct tm tm;
    localtime_r(&now, &tm);
    scene_set_clock(tm.tm_hour, tm.tm_min);
  }
  NetState ns = net_get();
  scene_set_weather(ns.weather_valid ? ns.weather : WX_UNKNOWN);
  scene_set_lights(g_pet.light_on);
  scene_set_poop(g_pet.poop);
}

void update_tilt(float dt) {
  const ImuState &imu = imu_state();
  if (!imu.ok || visit != VISIT_NONE) {
    pet_view_set_lean(pet, 0);
    return;
  }
  float gx = imu.gx;
  pet_view_set_lean(pet, gx * 1.8f);
  // Strong tilt makes the pet slide; it springs back to the centre when level.
  float accel = fabsf(gx) > 0.3f && !g_pet.asleep && g_pet.stage != STAGE_EGG ? gx * Sf(260) : -pet_offset * 6;
  pet_vel += accel * dt;
  pet_vel *= 0.9f;
  pet_offset += pet_vel * dt;
  float max_offset = SX(48);
  if (pet_offset > max_offset) pet_offset = max_offset, pet_vel = 0;
  if (pet_offset < -max_offset) pet_offset = -max_offset, pet_vel = 0;
  lv_obj_set_x(pet, pet_base_x + (int)pet_offset);
}

void update_visit(float dt) {
  if (visit == VISIT_NONE) return;
  visit_t += dt;
  int friend_home = DISPLAY_WIDTH - lv_obj_get_width(friend_pet) + S(26);
  switch (visit) {
    case VISIT_ENTER: {
      float k = visit_t / 1.2f;
      if (k > 1) k = 1;
      float ease = 1 - (1 - k) * (1 - k);
      lv_obj_set_x(friend_pet, DISPLAY_WIDTH + (int)((friend_home - DISPLAY_WIDTH) * ease));
      pet_offset = -Sf(30) * ease;
      lv_obj_set_x(pet, pet_base_x + (int)pet_offset);
      if (k >= 1) {
        visit = VISIT_PARTY;
        visit_t = 0;
        home_play(ANIM_DANCE);
        pet_view_play(friend_pet, ANIM_DANCE);
      }
      break;
    }
    case VISIT_PARTY:
      if (visit_t > 4.5f) {
        visit = VISIT_LEAVE;
        visit_t = 0;
      } else if (!pet_view_busy(pet)) {
        home_play(ANIM_HAPPY);
        pet_view_play(friend_pet, ANIM_HAPPY);
      }
      break;
    case VISIT_LEAVE: {
      float k = visit_t / 1.2f;
      if (k > 1) k = 1;
      lv_obj_set_x(friend_pet, friend_home + (int)((DISPLAY_WIDTH - friend_home) * k * k));
      pet_offset = -Sf(30) * (1 - k);
      lv_obj_set_x(pet, pet_base_x + (int)pet_offset);
      if (k >= 1) {
        visit = VISIT_NONE;
        lv_obj_add_flag(friend_pet, LV_OBJ_FLAG_HIDDEN);
      }
      break;
    }
    default: break;
  }
  pet_view_tick(friend_pet, (uint32_t)(dt * 1000));
}

void touch_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_indev_t *indev = lv_indev_get_act();
  if (!indev) return;
  lv_point_t p;
  lv_indev_get_point(indev, &p);
  uint32_t ms = millis();

  if (code == LV_EVENT_PRESSED) {
    press_swallowed = sheets_any_open() || visit != VISIT_NONE;
    if (sheets_any_open()) sheets_close_all();
    press_pt = last_pt = p;
    press_ms = ms;
    press_moved = false;
    press_on_pet = pet_view_hit(pet, p.x, p.y);
    stroke_acc = 0;
    return;
  }
  if (press_swallowed) return;

  if (code == LV_EVENT_PRESSING) {
    int dx = p.x - last_pt.x, dy = p.y - last_pt.y;
    float dist = sqrtf(dx * dx + dy * dy);
    last_pt = p;
    if (abs(p.x - press_pt.x) + abs(p.y - press_pt.y) > S(12)) press_moved = true;
    pet_view_look_at(pet, true, p.x, p.y);
    bool can_pet = g_pet.stage != STAGE_EGG && !g_pet.asleep;
    if (press_on_pet && can_pet && pet_view_hit(pet, p.x, p.y)) {
      stroke_acc += dist;
      if (stroke_acc > Sf(45) && ms - last_pat_ms > 300) {
        stroke_acc = 0;
        last_pat_ms = ms;
        app_pat();
        pet_view_emit_hearts(pet, 1);
        if (!pet_view_busy(pet)) home_play(ANIM_PAT);
      }
    }
  } else if (code == LV_EVENT_RELEASED) {
    pet_view_look_at(pet, false);
    if (press_moved || ms - press_ms > 450) return;
    int poop = scene_poop_at(p.x, p.y);
    if (poop >= 0) {
      if (app_clean()) {
        pet_view_emit_sparkles(pet, 8);
        ui_toast("Squeaky clean!", ICON_CLEAN, COL_HYGIENE);
      }
    } else if (press_on_pet) {
      if (g_pet.stage == STAGE_EGG)
        pet_view_emit_sparkles(pet, 2);
      else if (g_pet.asleep)
        ui_toast("Shh... sleeping", ICON_MOON, 0xE6ECFF);
      else {
        app_pat();
        home_play(ANIM_PAT);
      }
    } else if (p.y > DISPLAY_HEIGHT - S(40)) {
      sheets_open_menu();
    }
  } else if (code == LV_EVENT_GESTURE) {
    if (lv_event_get_target(e) != scr) return;  // bubbled up from a sheet
    if (lv_indev_get_gesture_dir(indev) == LV_DIR_TOP && !press_on_pet) {
      press_swallowed = true;
      sheets_open_menu();
    }
  }
}

void toast_hide_cb(lv_timer_t *) {
  lv_obj_add_flag(toast, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(name_label, LV_OBJ_FLAG_HIDDEN);
  toast_timer = nullptr;
}

}  // namespace

lv_obj_t *home_screen() { return scr; }
lv_obj_t *home_pet() { return pet; }
lv_obj_t *home_top_layer() { return top_layer; }
void home_refresh_hud() { refresh_hud(); }

void home_play(PetAnim anim) {
  if (g_pet.stage == STAGE_EGG && anim != ANIM_HATCH) return;
  pet_view_play(pet, anim);
}

void home_show() {
  sync_pet_look();
  refresh_hud();
  lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, true);  // deletes the game screen
}

void ui_init() {
  scr = lv_obj_create(nullptr);
  lv_obj_remove_style_all(scr);
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  scene_create_back(scr);

  pet = pet_view_create(scr, PET_SIZE);
  lv_obj_clear_flag(pet, LV_OBJ_FLAG_CLICKABLE);
  pet_base_x = (DISPLAY_WIDTH - lv_obj_get_width(pet)) / 2;
  lv_obj_set_pos(pet, pet_base_x, pet_view_top_for_feet(pet, SCENE_FEET_Y));

  friend_pet = pet_view_create(scr, FRIEND_SIZE);
  lv_obj_clear_flag(friend_pet, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(friend_pet, DISPLAY_WIDTH, pet_view_top_for_feet(friend_pet, SCENE_FEET_Y));
  pet_view_set_mirror(friend_pet, true);
  lv_obj_add_flag(friend_pet, LV_OBJ_FLAG_HIDDEN);

  scene_create_front(scr);
  build_hud();

  top_layer = ui_box(scr);
  lv_obj_set_size(top_layer, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  sheets_init(top_layer);

  lv_obj_add_event_cb(scr, touch_cb, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(scr, touch_cb, LV_EVENT_PRESSING, nullptr);
  lv_obj_add_event_cb(scr, touch_cb, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(scr, touch_cb, LV_EVENT_GESTURE, nullptr);

  strncpy(last_name, g_pet.name, PET_NAME_LEN);
  sync_pet_look();
  sync_scene();
  refresh_hud();
  lv_scr_load(scr);
}

void ui_frame(uint32_t dt_ms) {
  if (game_active()) {
    game_frame(dt_ms);
    return;
  }
  float dt = dt_ms / 1000.0f;
  sync_pet_look();
  sync_scene();
  update_tilt(dt);
  update_visit(dt);
  pet_view_tick(pet, dt_ms);
  scene_tick(dt_ms);
  sheets_frame(dt_ms);

  hud_timer += dt_ms;
  if (hud_timer >= 500) {
    hud_timer = 0;
    refresh_hud();
  }
}

void ui_toast(const char *msg, Icon icon, uint32_t color) {
  if (!toast) return;
  icon_set(toast_icon, icon, lv_color_hex(color));
  lv_label_set_text(toast_label, msg);
  lv_obj_clear_flag(toast, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(name_label, LV_OBJ_FLAG_HIDDEN);
  if (toast_timer) lv_timer_del(toast_timer);
  toast_timer = lv_timer_create(toast_hide_cb, TOAST_MS, nullptr);
  lv_timer_set_repeat_count(toast_timer, 1);
}

bool ui_idle_on_home() { return !game_active() && !sheets_any_open() && visit == VISIT_NONE; }

void ui_on_events(uint32_t ev) {
  char buf[64];
  if (ev & EVT_LEFT) {
    snprintf(buf, sizeof(buf), "%s went on an adventure. A new egg appeared!", last_name);
    ui_toast(buf, ICON_HEART, COL_HAPPY);
    power_pulse();
  }
  strncpy(last_name, g_pet.name, PET_NAME_LEN);
  sync_pet_look();

  if (ev & EVT_HATCHED) {
    home_play(ANIM_HATCH);
    snprintf(buf, sizeof(buf), "%s hatched!", g_pet.name);
    ui_toast(buf, ICON_CLEAN, COL_COIN);
    power_activity();
  } else if (ev & EVT_EVOLVED) {
    home_play(ANIM_HATCH);
    snprintf(buf, sizeof(buf), "Evolved into %s!", pet_form_name(g_pet.form));
    ui_toast(buf, ICON_CLEAN, COL_COIN);
    power_activity();
  } else if (ev & EVT_GOT_SICK) {
    snprintf(buf, sizeof(buf), "%s feels sick...", g_pet.name);
    ui_toast(buf, ICON_PILL, COL_WARN);
    power_pulse();
  } else if (ev & EVT_WOKE_UP) {
    home_play(ANIM_HAPPY);
    ui_toast("Good morning!", ICON_SUN, 0xFFE066);
  } else if (ev & EVT_FELL_ASLEEP) {
    if (g_pet.light_on) ui_toast("Sleepy... lights off?", ICON_BULB, COL_ENERGY);
  } else if (ev & EVT_POOPED) {
    ui_toast("Oops! Tap to clean", ICON_CLEAN, COL_HYGIENE);
    power_pulse();
  } else if (ev & EVT_NEEDS_ATTENTION) {
    if (g_pet.hunger < 20)
      ui_toast("I'm hungry!", ICON_FOOD, COL_HUNGER);
    else if (g_pet.happiness < 20)
      ui_toast("Play with me?", ICON_HEART, COL_HAPPY);
    else
      ui_toast("Needs some care", ICON_HEART, COL_WARN);
    power_pulse();
  }
}

void ui_on_gestures(uint8_t g) {
  if (game_active() || g_pet.stage == STAGE_EGG) return;
  if (g & IMU_SHAKE) {
    bool was_asleep = g_pet.asleep;
    app_shaken();
    if (was_asleep) {
      home_play(ANIM_WAKE);
      ui_toast("Hey! I was sleeping!", ICON_MOON, 0xE6ECFF);
    } else {
      home_play(ANIM_DIZZY);
    }
  } else if ((g & IMU_PICKUP) && !g_pet.asleep && !pet_view_busy(pet)) {
    home_play(ANIM_SURPRISED);
  }
  if ((g & IMU_FACE_DOWN) && g_pet.light_on && g_pet.asleep) {
    app_toggle_light();
    ui_toast("Lights out. Night night!", ICON_MOON, 0xE6ECFF);
  }
}

void ui_friend_visit(const FriendSeen &f, uint32_t gift, bool first_time) {
  visitor = f;
  pet_view_set_look(friend_pet, f.form, f.palette, f.stage);
  pet_view_set_mood(friend_pet, MOOD_HAPPY);
  lv_obj_set_x(friend_pet, DISPLAY_WIDTH);
  lv_obj_clear_flag(friend_pet, LV_OBJ_FLAG_HIDDEN);
  visit = VISIT_ENTER;
  visit_t = 0;
  power_activity();
  char buf[64];
  snprintf(buf, sizeof(buf), first_time ? "New friend %s! +%lu" : "%s came to visit! +%lu", f.name, (unsigned long)gift);
  ui_toast(buf, ICON_FRIENDS, COL_HAPPY);
}
