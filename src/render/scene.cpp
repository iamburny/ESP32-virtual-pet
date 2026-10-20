#include "scene.h"

#include <math.h>

#include "display.h"
#include "draw2d.h"
#include "net/net_task.h"

#define W DISPLAY_WIDTH
#define H DISPLAY_HEIGHT
#define MAX_DROPS 22
#define MAX_CLOUDS 4
#define STAR_COUNT 18

namespace {

struct SkyKey {
  float hour;
  uint32_t top, bottom;
};
const SkyKey SKY[] = {
    {0, 0x0B1026, 0x1B2346},  {5, 0x111836, 0x3A2F5E},   {6.5f, 0x5B7BC9, 0xFFB38A}, {8.5f, 0x4FA3F7, 0xBDE6FF},
    {17, 0x4FA3F7, 0xBDE6FF}, {19, 0x3D4C9E, 0xFF8A65},  {20.5f, 0x1B2346, 0x3B2F5E}, {24, 0x0B1026, 0x1B2346},
};

struct Drop {
  float x, y, speed, phase;
  uint8_t size;
};

struct Cloud {
  float x, y, scale, speed;
};

lv_obj_t *back_obj, *front_obj;
float hour_f = 12;
uint8_t weather;
bool lights_on = true;
int poop_count;
float t;

Drop drops[MAX_DROPS];
int drop_count;
Cloud clouds[MAX_CLOUDS];
int cloud_count;
float fog_x;
float flash, next_flash = 5;
float cloud_redraw;
uint32_t rng = 12345;

const int POOP_X[] = {28, 146, 50, 124};

float frand() {
  rng = rng * 1664525u + 1013904223u;
  return (rng >> 8) / 16777216.0f;
}

lv_color_t sky_color(bool top) {
  for (unsigned i = 0; i + 1 < sizeof(SKY) / sizeof(SKY[0]); i++) {
    const SkyKey &a = SKY[i], &b = SKY[i + 1];
    if (hour_f >= a.hour && hour_f <= b.hour) {
      float k = (hour_f - a.hour) / (b.hour - a.hour);
      return d2::mix(lv_color_hex(top ? a.top : a.bottom), lv_color_hex(top ? b.top : b.bottom), k);
    }
  }
  return lv_color_hex(top ? SKY[0].top : SKY[0].bottom);
}

bool overcast() { return weather == WX_CLOUDY || weather == WX_RAIN || weather == WX_STORM || weather == WX_DRIZZLE || weather == WX_FOG || weather == WX_SNOW; }

float night_k() {
  // 0 during the day, ramps through dawn (5-7) and dusk (19-21).
  if (hour_f >= 7 && hour_f <= 19) return 0;
  if (hour_f > 19 && hour_f < 21) return (hour_f - 19) / 2;
  if (hour_f > 5 && hour_f < 7) return 1 - (hour_f - 5) / 2;
  return 1;
}

void draw_cloud(lv_draw_ctx_t *ctx, float x, float y, float s, lv_color_t c, lv_opa_t opa) {
  d2::circle(ctx, x + 10 * s, y + 4 * s, 10 * s, c, opa);
  d2::circle(ctx, x + 24 * s, y - 2 * s, 13 * s, c, opa);
  d2::circle(ctx, x + 38 * s, y + 5 * s, 9 * s, c, opa);
  d2::rect(ctx, x + 2 * s, y + 4 * s, 44 * s, 11 * s, 6 * s, c, opa);
}

void draw_poop(lv_draw_ctx_t *ctx, int cx, int by) {
  lv_color_t c = lv_color_hex(0x8B5A3C), hl = lv_color_hex(0xB07A52);
  d2::oval(ctx, cx, by + 2, 13, 3, lv_color_black(), LV_OPA_20);
  d2::oval(ctx, cx, by - 4, 12, 6, c);
  d2::oval(ctx, cx, by - 11, 9, 5, c);
  d2::oval(ctx, cx + 1, by - 17, 5, 4, c);
  d2::oval(ctx, cx - 4, by - 12, 3, 2, hl);
  d2::circle(ctx, cx - 4, by - 5, 2, lv_color_white());
  d2::circle(ctx, cx + 4, by - 5, 2, lv_color_white());
  d2::circle(ctx, cx - 4, by - 5, 1, lv_color_black());
  d2::circle(ctx, cx + 4, by - 5, 1, lv_color_black());
}

void back_draw_cb(lv_event_t *e) {
  lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
  float nk = night_k();
  bool dull = overcast();

  lv_color_t top = sky_color(true), bottom = sky_color(false);
  if (dull) {
    top = d2::mix(top, lv_color_hex(0x7D8796), 0.5f * (1 - nk * 0.7f));
    bottom = d2::mix(bottom, lv_color_hex(0xA9B1BC), 0.5f * (1 - nk * 0.7f));
  }
  d2::grad_rect(ctx, 0, 0, W, SCENE_GROUND_Y + 10, 0, top, bottom);

  // Stars.
  if (nk > 0.05f && !dull) {
    uint32_t s = 777;
    for (int i = 0; i < STAR_COUNT; i++) {
      s = s * 1103515245u + 12345u;
      int x = (s >> 8) % W;
      s = s * 1103515245u + 12345u;
      int y = 8 + (s >> 8) % (SCENE_GROUND_Y - 70);
      float tw = 0.55f + 0.45f * sinf(t * (1.3f + (i % 5) * 0.4f) + i);
      d2::circle(ctx, x, y, (i % 4 == 0) ? 2 : 1, lv_color_white(), (lv_opa_t)(255 * nk * tw));
    }
  }

  // Sun by day, moon by night, following an arc across the sky.
  bool is_day = hour_f >= 6 && hour_f < 20;
  float p = is_day ? (hour_f - 6) / 14 : fmodf(hour_f + 4, 24) / 10;
  float bx = 18 + p * (W - 36);
  float by = 150 - sinf(p * (float)M_PI) * 105;
  if (is_day) {
    lv_opa_t o = dull ? LV_OPA_30 : LV_OPA_COVER;
    d2::circle(ctx, bx, by, 24, lv_color_hex(0xFFE9A0), (lv_opa_t)(o / 4));
    d2::circle(ctx, bx, by, 17, lv_color_hex(0xFFE9A0), (lv_opa_t)(o / 3));
    d2::circle(ctx, bx, by, 12, lv_color_hex(0xFFD23F), o);
  } else {
    d2::circle(ctx, bx, by, 18, lv_color_hex(0xDDE6FF), LV_OPA_10);
    d2::circle(ctx, bx, by, 11, lv_color_hex(0xF4F1E0));
    d2::circle(ctx, bx + 5, by - 3, 9, top);  // crescent
  }

  // Clouds.
  lv_color_t cc = d2::mix(lv_color_white(), lv_color_hex(0x6B7484), dull ? 0.45f : 0);
  cc = d2::mix(cc, lv_color_hex(0x2A3150), nk * 0.75f);
  for (int i = 0; i < cloud_count; i++)
    draw_cloud(ctx, clouds[i].x, clouds[i].y, clouds[i].scale, cc, dull ? 230 : 210);

  // Hills and ground.
  lv_color_t night_tint = lv_color_hex(0x16213E);
  lv_color_t hill = d2::mix(lv_color_hex(0x8FD19E), night_tint, nk * 0.7f);
  lv_color_t hill2 = d2::mix(lv_color_hex(0x6CC283), night_tint, nk * 0.7f);
  d2::oval(ctx, 20, SCENE_GROUND_Y + 8, 90, 34, hill);
  d2::oval(ctx, 160, SCENE_GROUND_Y + 12, 80, 28, hill2);
  lv_color_t g_top = d2::mix(lv_color_hex(0x7BD389), night_tint, nk * 0.72f);
  lv_color_t g_bot = d2::mix(lv_color_hex(0x3E9E5C), night_tint, nk * 0.78f);
  d2::grad_rect(ctx, 0, SCENE_GROUND_Y, W, H - SCENE_GROUND_Y, 0, g_top, g_bot);
  d2::rect(ctx, 0, SCENE_GROUND_Y, W, 3, 0, lv_color_white(), LV_OPA_20);

  // Grass tufts and flowers.
  lv_color_t tuft = d2::mix(lv_color_hex(0x4FB06A), night_tint, nk * 0.6f);
  const int tufts[][2] = {{12, 250}, {64, 244}, {108, 296}, {156, 252}, {30, 300}, {92, 248}, {140, 304}};
  for (auto &tf : tufts) {
    d2::line(ctx, tf[0], tf[1], tf[0] - 3, tf[1] - 7, 2, tuft);
    d2::line(ctx, tf[0], tf[1], tf[0] + 1, tf[1] - 9, 2, tuft);
    d2::line(ctx, tf[0], tf[1], tf[0] + 4, tf[1] - 6, 2, tuft);
  }
  lv_opa_t fo = (lv_opa_t)(255 * (1 - nk * 0.6f));
  d2::circle(ctx, 150, 286, 3, lv_color_hex(0xFFD23F), fo);
  d2::circle(ctx, 20, 276, 3, lv_color_hex(0xFF8FB1), fo);
  d2::circle(ctx, 78, 306, 3, lv_color_hex(0xFFFFFF), fo);

  for (int i = 0; i < poop_count && i < 4; i++) draw_poop(ctx, POOP_X[i], SCENE_FEET_Y + 4 + (i >= 2 ? 26 : 0));
}

void front_draw_cb(lv_event_t *e) {
  lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);

  if (!lights_on) d2::rect(ctx, 0, 0, W, H, 0, lv_color_hex(0x05060F), 165);

  switch (weather) {
    case WX_RAIN:
    case WX_DRIZZLE:
    case WX_STORM:
      for (int i = 0; i < drop_count; i++) {
        const Drop &d = drops[i];
        d2::line(ctx, d.x, d.y, d.x - 2, d.y + d.size, 1, lv_color_hex(0xCFE6FF), LV_OPA_70);
      }
      break;
    case WX_SNOW:
      for (int i = 0; i < drop_count; i++) {
        const Drop &d = drops[i];
        d2::circle(ctx, d.x + sinf(t * 1.5f + d.phase) * 4, d.y, d.size, lv_color_white(), LV_OPA_90);
      }
      break;
    case WX_FOG:
      for (int i = 0; i < 3; i++) {
        float x = fmodf(fog_x * (1 + i * 0.3f) + i * 70, W + 120) - 120;
        d2::rect(ctx, x, 120 + i * 50, 150, 22, 11, lv_color_white(), LV_OPA_30);
      }
      break;
    default: break;
  }

  if (flash > 0) d2::rect(ctx, 0, 0, W, H, 0, lv_color_white(), (lv_opa_t)(200 * flash));
}

void reset_weather_particles() {
  switch (weather) {
    case WX_RAIN:
    case WX_STORM: drop_count = MAX_DROPS; break;
    case WX_DRIZZLE: drop_count = MAX_DROPS / 2; break;
    case WX_SNOW: drop_count = MAX_DROPS; break;
    default: drop_count = 0; break;
  }
  bool snow = weather == WX_SNOW;
  for (int i = 0; i < drop_count; i++) {
    drops[i].x = frand() * (W + 20);
    drops[i].y = frand() * H;
    drops[i].speed = snow ? 18 + frand() * 22 : (weather == WX_DRIZZLE ? 160 : 260) + frand() * 80;
    drops[i].size = snow ? 1 + (uint8_t)(frand() * 2.5f) : 7 + (uint8_t)(frand() * 6);
    drops[i].phase = frand() * 6.28f;
  }

  switch (weather) {
    case WX_CLEAR:
    case WX_UNKNOWN: cloud_count = 1; break;
    case WX_PARTLY: cloud_count = 2; break;
    default: cloud_count = MAX_CLOUDS; break;
  }
  for (int i = 0; i < cloud_count; i++) {
    clouds[i].x = frand() * W - 20;
    clouds[i].y = 30 + i * 32 + frand() * 15;
    clouds[i].scale = 0.7f + frand() * 0.5f;
    clouds[i].speed = 2 + frand() * 3;
  }
}

void invalidate_rect(lv_obj_t *obj, int x, int y, int w, int h) {
  lv_area_t a = {(lv_coord_t)x, (lv_coord_t)y, (lv_coord_t)(x + w), (lv_coord_t)(y + h)};
  lv_obj_invalidate_area(obj, &a);
}

lv_obj_t *make_layer(lv_obj_t *parent, lv_event_cb_t cb) {
  lv_obj_t *obj = lv_obj_create(parent);
  lv_obj_remove_style_all(obj);
  lv_obj_set_size(obj, W, H);
  lv_obj_set_pos(obj, 0, 0);
  lv_obj_clear_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  lv_obj_add_event_cb(obj, cb, LV_EVENT_DRAW_MAIN, nullptr);
  return obj;
}

}  // namespace

lv_obj_t *scene_create_back(lv_obj_t *parent) {
  back_obj = make_layer(parent, back_draw_cb);
  reset_weather_particles();
  return back_obj;
}

lv_obj_t *scene_create_front(lv_obj_t *parent) {
  front_obj = make_layer(parent, front_draw_cb);
  return front_obj;
}

void scene_set_clock(int hour, int minute) {
  float h = hour + minute / 60.0f;
  if (fabsf(h - hour_f) < 0.01f) return;
  hour_f = h;
  if (back_obj) lv_obj_invalidate(back_obj);
}

void scene_set_weather(uint8_t wx) {
  if (wx == weather) return;
  weather = wx;
  reset_weather_particles();
  if (back_obj) lv_obj_invalidate(back_obj);
  if (front_obj) lv_obj_invalidate(front_obj);
}

void scene_set_lights(bool on) {
  if (on == lights_on) return;
  lights_on = on;
  if (front_obj) lv_obj_invalidate(front_obj);
}

void scene_set_poop(int count) {
  if (count == poop_count) return;
  poop_count = count;
  if (back_obj) invalidate_rect(back_obj, 0, SCENE_FEET_Y - 22, W, 56);
}

int scene_poop_at(int x, int y) {
  for (int i = 0; i < poop_count && i < 4; i++) {
    int by = SCENE_FEET_Y + 4 + (i >= 2 ? 26 : 0);
    if (abs(x - POOP_X[i]) < 18 && y > by - 26 && y < by + 8) return i;
  }
  return -1;
}

float scene_night() { return night_k(); }

void scene_tick(uint32_t dt_ms) {
  if (!back_obj || !front_obj) return;
  float dt = dt_ms / 1000.0f;
  t += dt;

  // Clouds drift slowly; redraw their strip a few times a second.
  for (int i = 0; i < cloud_count; i++) {
    clouds[i].x += clouds[i].speed * dt;
    if (clouds[i].x > W + 10) clouds[i].x = -60 * clouds[i].scale;
  }
  cloud_redraw += dt;
  if (cloud_redraw > 0.25f) {
    cloud_redraw = 0;
    for (int i = 0; i < cloud_count; i++) {
      float s = clouds[i].scale;
      invalidate_rect(back_obj, clouds[i].x - 4, clouds[i].y - 16 * s, 56 * s, 34 * s);
    }
    if (night_k() > 0.05f && !overcast()) invalidate_rect(back_obj, 0, 0, W, SCENE_GROUND_Y - 60);  // twinkle
  }

  // Each drop invalidates the strip covering its old and new position.
  bool snow = weather == WX_SNOW;
  for (int i = 0; i < drop_count; i++) {
    Drop &d = drops[i];
    float oy = d.y;
    d.y += d.speed * dt;
    if (!snow) d.x -= d.speed * dt * 0.08f;
    if (d.y > H) {
      invalidate_rect(front_obj, d.x - 12, oy - 10, 24, H - oy + 20);
      d.y = -10;
      d.x = frand() * (W + 20);
      oy = d.y;
    }
    int pad = snow ? 8 : 3;
    invalidate_rect(front_obj, d.x - pad - 2, oy - pad, pad * 2 + 4, d.y - oy + d.size + pad * 2);
  }

  if (weather == WX_FOG) {
    fog_x += 6 * dt;
    invalidate_rect(front_obj, 0, 110, W, 150);
  }

  if (weather == WX_STORM) {
    next_flash -= dt;
    if (next_flash <= 0) {
      flash = 1;
      next_flash = 4 + frand() * 6;
    }
  }
  if (flash > 0) {
    flash -= dt * 4;
    if (flash < 0) flash = 0;
    lv_obj_invalidate(front_obj);
  }
}
