#include "scene.h"

#include <math.h>

#include "display.h"
#include "draw2d.h"
#include "layout.h"
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
  lv_area_t drawn;  // box invalidated at the last redraw
  bool drawn_valid;
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

// Poop slots, in 172 px design units.
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

// Fixed pseudo-random star positions.
void star_pos(int i, int &x, int &y) {
  uint32_t s = 777 + i * 2654435761u;
  s = s * 1103515245u + 12345u;
  x = (s >> 8) % W;
  s = s * 1103515245u + 12345u;
  y = S(8) + (s >> 8) % (SCENE_GROUND_Y - S(70));
}

void draw_cloud(lv_draw_ctx_t *ctx, float x, float y, float s, lv_color_t c, lv_opa_t opa) {
  d2::circle(ctx, x + 10 * s, y + 4 * s, 10 * s, c, opa);
  d2::circle(ctx, x + 24 * s, y - 2 * s, 13 * s, c, opa);
  d2::circle(ctx, x + 38 * s, y + 5 * s, 9 * s, c, opa);
  d2::rect(ctx, x + 2 * s, y + 4 * s, 44 * s, 11 * s, 6 * s, c, opa);
}

int poop_x(int i) { return SX(POOP_X[i]); }
int poop_y(int i) { return SCENE_FEET_Y + S(4) + (i >= 2 ? S(26) : 0); }

void draw_poop(lv_draw_ctx_t *ctx, int cx, int by) {
  lv_color_t c = lv_color_hex(0x8B5A3C), hl = lv_color_hex(0xB07A52);
  d2::oval(ctx, cx, by + S(2), S(13), S(3), lv_color_black(), LV_OPA_20);
  d2::oval(ctx, cx, by - S(4), S(12), S(6), c);
  d2::oval(ctx, cx, by - S(11), S(9), S(5), c);
  d2::oval(ctx, cx + S(1), by - S(17), S(5), S(4), c);
  d2::oval(ctx, cx - S(4), by - S(12), S(3), S(2), hl);
  d2::circle(ctx, cx - S(4), by - S(5), S(2), lv_color_white());
  d2::circle(ctx, cx + S(4), by - S(5), S(2), lv_color_white());
  d2::circle(ctx, cx - S(4), by - S(5), S(1), lv_color_black());
  d2::circle(ctx, cx + S(4), by - S(5), S(1), lv_color_black());
}

lv_color_t sky_top() {
  lv_color_t top = sky_color(true);
  if (overcast()) top = d2::mix(top, lv_color_hex(0x7D8796), 0.5f * (1 - night_k() * 0.7f));
  return top;
}

// What the time of day contributes to the backdrop.
struct SkyState {
  lv_color_t top, bottom;
  float nk;
  bool day;
  float sun_x, sun_y;  // sun by day, moon by night, following an arc across the sky
};

SkyState sky_state() {
  SkyState st;
  st.nk = night_k();
  st.top = sky_top();
  st.bottom = sky_color(false);
  if (overcast()) st.bottom = d2::mix(st.bottom, lv_color_hex(0xA9B1BC), 0.5f * (1 - st.nk * 0.7f));
  st.day = hour_f >= 6 && hour_f < 20;
  float p = st.day ? (hour_f - 6) / 14 : fmodf(hour_f + 4, 24) / 10;
  st.sun_x = S(18) + p * (W - S(36));
  st.sun_y = S(150) - sinf(p * (float)M_PI) * S(105);
  return st;
}

// The parts of the backdrop that only change with the hour and the weather: sky,
// sun or moon, hills, ground, grass and flowers.
void draw_static(lv_draw_ctx_t *ctx) {
  SkyState sky = sky_state();
  float nk = sky.nk;
  bool dull = overcast();
  lv_color_t top = sky.top;
  d2::grad_rect(ctx, 0, 0, W, SCENE_GROUND_Y + 10, 0, top, sky.bottom);

  float bx = sky.sun_x, by = sky.sun_y;
  if (sky.day) {
    lv_opa_t o = dull ? LV_OPA_30 : LV_OPA_COVER;
    d2::circle(ctx, bx, by, S(24), lv_color_hex(0xFFE9A0), (lv_opa_t)(o / 4));
    d2::circle(ctx, bx, by, S(17), lv_color_hex(0xFFE9A0), (lv_opa_t)(o / 3));
    d2::circle(ctx, bx, by, S(12), lv_color_hex(0xFFD23F), o);
  } else {
    d2::circle(ctx, bx, by, S(18), lv_color_hex(0xDDE6FF), LV_OPA_10);
    d2::circle(ctx, bx, by, S(11), lv_color_hex(0xF4F1E0));
    d2::circle(ctx, bx + S(5), by - S(3), S(9), top);  // crescent
  }

  // Hills and ground.
  lv_color_t night_tint = lv_color_hex(0x16213E);
  lv_color_t hill = d2::mix(lv_color_hex(0x8FD19E), night_tint, nk * 0.7f);
  lv_color_t hill2 = d2::mix(lv_color_hex(0x6CC283), night_tint, nk * 0.7f);
  d2::oval(ctx, SX(20), SCENE_GROUND_Y + S(8), SX(90), S(34), hill);
  d2::oval(ctx, SX(160), SCENE_GROUND_Y + S(12), SX(80), S(28), hill2);
  lv_color_t g_top = d2::mix(lv_color_hex(0x7BD389), night_tint, nk * 0.72f);
  lv_color_t g_bot = d2::mix(lv_color_hex(0x3E9E5C), night_tint, nk * 0.78f);
  d2::grad_rect(ctx, 0, SCENE_GROUND_Y, W, H - SCENE_GROUND_Y, 0, g_top, g_bot);
  d2::rect(ctx, 0, SCENE_GROUND_Y, W, S(3), 0, lv_color_white(), LV_OPA_20);

  // Grass tufts and flowers.
  lv_color_t tuft = d2::mix(lv_color_hex(0x4FB06A), night_tint, nk * 0.6f);
  const int tufts[][2] = {{12, 250}, {64, 244}, {108, 296}, {156, 252}, {30, 300}, {92, 248}, {140, 304}};
  for (auto &tf : tufts) {
    int x = SX(tf[0]), y = S(tf[1]);
    d2::line(ctx, x, y, x - S(3), y - S(7), S(2), tuft);
    d2::line(ctx, x, y, x + S(1), y - S(9), S(2), tuft);
    d2::line(ctx, x, y, x + S(4), y - S(6), S(2), tuft);
  }
  lv_opa_t fo = (lv_opa_t)(255 * (1 - nk * 0.6f));
  d2::circle(ctx, SX(150), S(286), S(3), lv_color_hex(0xFFD23F), fo);
  d2::circle(ctx, SX(20), S(276), S(3), lv_color_hex(0xFF8FB1), fo);
  d2::circle(ctx, SX(78), S(306), S(3), lv_color_hex(0xFFFFFF), fo);
}

#if defined(BOARD_HAS_PSRAM)
// With PSRAM to spare, the static backdrop is rendered once into an image and
// copied under each dirty area, instead of redrawing its gradients and
// anti-aliased hills every time; it is rebuilt when the hour or weather changes.
lv_color_t *bg_cache;
lv_img_dsc_t bg_img;
bool bg_valid;

// Renders draw_static() into bg_cache through an offscreen draw context, the
// same way lv_snapshot renders an object to a buffer.
void build_bg_cache() {
  if (!bg_cache) bg_cache = (lv_color_t *)heap_caps_malloc(W * H * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
  if (!bg_cache) return;
  lv_area_t area = {0, 0, W - 1, H - 1};
  lv_disp_t *disp = lv_disp_get_default();
  lv_disp_drv_t driver;
  lv_disp_drv_init(&driver);
  driver.hor_res = W;
  driver.ver_res = H;
  lv_disp_t fake_disp;
  memset(&fake_disp, 0, sizeof(fake_disp));
  fake_disp.driver = &driver;
  lv_draw_ctx_t *ctx = (lv_draw_ctx_t *)lv_mem_alloc(disp->driver->draw_ctx_size);
  if (!ctx) return;
  disp->driver->draw_ctx_init(&driver, ctx);
  driver.draw_ctx = ctx;
  ctx->clip_area = &area;
  ctx->buf_area = &area;
  ctx->buf = bg_cache;

  lv_disp_t *refreshing = _lv_refr_get_disp_refreshing();
  _lv_refr_set_disp_refreshing(&fake_disp);
  draw_static(ctx);
  _lv_refr_set_disp_refreshing(refreshing);
  disp->driver->draw_ctx_deinit(&driver, ctx);
  lv_mem_free(ctx);

  memset(&bg_img, 0, sizeof(bg_img));
  bg_img.header.cf = LV_IMG_CF_TRUE_COLOR;
  bg_img.header.w = W;
  bg_img.header.h = H;
  bg_img.data_size = W * H * sizeof(lv_color_t);
  bg_img.data = (const uint8_t *)bg_cache;
  bg_valid = true;
}

void invalidate_bg_cache() { bg_valid = false; }

void draw_backdrop(lv_draw_ctx_t *ctx) {
  if (!bg_valid) {
    draw_static(ctx);
    return;
  }
  // The backdrop is opaque and drawn first, so with no mask active its pixels can
  // be copied row by row straight into the draw buffer; that is much cheaper per
  // dirty area than going through lv_draw_img.
  const lv_area_t *clip = ctx->clip_area;
  if (!lv_draw_mask_is_any(clip)) {
    const lv_area_t *buf_area = ctx->buf_area;
    int stride = lv_area_get_width(buf_area);
    int w = lv_area_get_width(clip);
    lv_color_t *dst = (lv_color_t *)ctx->buf + (clip->y1 - buf_area->y1) * stride + (clip->x1 - buf_area->x1);
    const lv_color_t *src = bg_cache + clip->y1 * W + clip->x1;
    for (int y = clip->y1; y <= clip->y2; y++, dst += stride, src += W) memcpy(dst, src, w * sizeof(lv_color_t));
    return;
  }
  lv_draw_img_dsc_t dsc;
  lv_draw_img_dsc_init(&dsc);
  lv_area_t coords = {0, 0, W - 1, H - 1};
  lv_draw_img(ctx, &dsc, &coords, &bg_img);
}
#else
void invalidate_bg_cache() {}
void draw_backdrop(lv_draw_ctx_t *ctx) { draw_static(ctx); }
#endif

void back_draw_cb(lv_event_t *e) {
  lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
  float nk = night_k();
  bool dull = overcast();

  draw_backdrop(ctx);

  // Stars.
  if (nk > 0.05f && !dull) {
    for (int i = 0; i < STAR_COUNT; i++) {
      int x, y;
      star_pos(i, x, y);
      float tw = 0.55f + 0.45f * sinf(t * (1.3f + (i % 5) * 0.4f) + i);
      d2::circle(ctx, x, y, (i % 4 == 0) ? S(2) : S(1), lv_color_white(), (lv_opa_t)(255 * nk * tw));
    }
  }

  // Clouds.
  lv_color_t cc = d2::mix(lv_color_white(), lv_color_hex(0x6B7484), dull ? 0.45f : 0);
  cc = d2::mix(cc, lv_color_hex(0x2A3150), nk * 0.75f);
  for (int i = 0; i < cloud_count; i++)
    draw_cloud(ctx, clouds[i].x, clouds[i].y, clouds[i].scale, cc, dull ? 230 : 210);

  for (int i = 0; i < poop_count && i < 4; i++) draw_poop(ctx, poop_x(i), poop_y(i));
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
        // A plain vertical bar: unlike an anti-aliased slanted line it needs no masks.
        d2::rect(ctx, d.x, d.y, LV_MAX(1, S(1)), d.size, 0, lv_color_hex(0xCFE6FF), LV_OPA_70);
      }
      break;
    case WX_SNOW:
      for (int i = 0; i < drop_count; i++) {
        const Drop &d = drops[i];
        d2::circle(ctx, d.x + sinf(t * 1.5f + d.phase) * Sf(4), d.y, d.size, lv_color_white(), LV_OPA_90);
      }
      break;
    case WX_FOG:
      for (int i = 0; i < 3; i++) {
        float x = fmodf(fog_x * (1 + i * 0.3f) + i * SX(70), W + SX(120)) - SX(120);
        d2::rect(ctx, x, S(120) + i * S(50), SX(150), S(22), S(11), lv_color_white(), LV_OPA_30);
      }
      break;
    default: break;
  }

  if (flash > 0) d2::rect(ctx, 0, 0, W, H, 0, lv_color_white(), (lv_opa_t)(200 * flash));
}

// Pixels a drop covers as drawn by front_draw_cb: a vertical rain streak, or a
// snowflake swaying up to Sf(4) either side plus 2 px for anti-aliasing.
lv_area_t drop_bounds(const Drop &d, bool snow) {
  float x1, x2, y1, y2;
  if (snow) {
    float reach = Sf(4) + d.size + 2;
    x1 = d.x - reach, x2 = d.x + reach;
    y1 = d.y - d.size - 2, y2 = d.y + d.size + 2;
  } else {
    x1 = d.x - 1, x2 = d.x + LV_MAX(1, S(1)) + 1;
    y1 = d.y - 1, y2 = d.y + d.size + 1;
  }
  return {(lv_coord_t)floorf(x1), (lv_coord_t)floorf(y1), (lv_coord_t)ceilf(x2), (lv_coord_t)ceilf(y2)};
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
    drops[i].speed = Sf(snow ? 18 + frand() * 22 : (weather == WX_DRIZZLE ? 160 : 260) + frand() * 80);
    drops[i].size = S(snow ? 1 + (int)(frand() * 2.5f) : 7 + (int)(frand() * 6));
    drops[i].phase = frand() * 6.28f;
  }

  switch (weather) {
    case WX_CLEAR:
    case WX_UNKNOWN: cloud_count = 1; break;
    case WX_PARTLY: cloud_count = 2; break;
    default: cloud_count = MAX_CLOUDS; break;
  }
  for (int i = 0; i < cloud_count; i++) {
    clouds[i].x = frand() * W - S(20);
    clouds[i].y = Sf(30 + i * 32 + frand() * 15);
    clouds[i].scale = UI_SCALE * (0.7f + frand() * 0.5f);
    clouds[i].speed = Sf(2 + frand() * 3);
    clouds[i].drawn_valid = false;
  }
}

void invalidate_rect(lv_obj_t *obj, int x, int y, int w, int h) {
  lv_area_t a = {(lv_coord_t)x, (lv_coord_t)y, (lv_coord_t)(x + w), (lv_coord_t)(y + h)};
  d2::invalidate(obj, a);
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
  SkyState before = sky_state();
  hour_f = h;
  SkyState after = sky_state();
  invalidate_bg_cache();
  if (!back_obj) return;

  // Most minutes only the sun or moon moves a little: redraw just where it was and
  // where it is, rather than the whole screen (a visible hitch on the big panel).
  bool same_sky = before.top.full == after.top.full && before.bottom.full == after.bottom.full &&
                  before.nk == after.nk && before.day == after.day;
  if (!same_sky) {
    lv_obj_invalidate(back_obj);
    return;
  }
  int r = S(24) + 2;  // largest sun/moon circle plus anti-aliasing
  for (const SkyState &st : {before, after})
    d2::invalidate(back_obj, {(lv_coord_t)(st.sun_x - r), (lv_coord_t)(st.sun_y - r), (lv_coord_t)(st.sun_x + r),
                              (lv_coord_t)(st.sun_y + r)});
}

void scene_set_weather(uint8_t wx) {
  if (wx == weather) return;
  weather = wx;
  invalidate_bg_cache();
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
  // From the top of the upper poops to the bottom of the lower poops' shadows.
  int top = poop_y(0) - S(22), bottom = poop_y(2) + S(6);
  if (back_obj) invalidate_rect(back_obj, 0, top, W, bottom - top);
}

int scene_poop_at(int x, int y) {
  for (int i = 0; i < poop_count && i < 4; i++) {
    int by = poop_y(i);
    if (abs(x - poop_x(i)) < S(18) && y > by - S(26) && y < by + S(8)) return i;
  }
  return -1;
}

float scene_night() { return night_k(); }

void scene_tick(uint32_t dt_ms) {
  if (!back_obj || !front_obj) return;
  float dt = dt_ms / 1000.0f;
  t += dt;
#if defined(BOARD_HAS_PSRAM)
  if (!bg_valid) build_bg_cache();
#endif

  // Clouds drift slowly; redraw their strip a few times a second.
  for (int i = 0; i < cloud_count; i++) {
    clouds[i].x += clouds[i].speed * dt;
    if (clouds[i].x > W + S(10)) clouds[i].x = -60 * clouds[i].scale;
  }
  cloud_redraw += dt;
  if (cloud_redraw > 0.25f) {
    cloud_redraw = 0;
    // Each cloud redraws the union of where it was last drawn and where it is now,
    // so a long gap between redraws can't strand its trailing edge.
    for (int i = 0; i < cloud_count; i++) {
      Cloud &c = clouds[i];
      float s = c.scale;
      lv_area_t box = {(lv_coord_t)(c.x - 4), (lv_coord_t)(c.y - 16 * s), (lv_coord_t)(c.x - 4 + 56 * s),
                       (lv_coord_t)(c.y + 18 * s)};
      lv_area_t both = box;
      if (c.drawn_valid && _lv_area_is_on(&box, &c.drawn)) {
        _lv_area_join(&both, &box, &c.drawn);
      } else if (c.drawn_valid) {
        d2::invalidate(back_obj, c.drawn);
      }
      d2::invalidate(back_obj, both);
      c.drawn = box;
      c.drawn_valid = true;
    }
    if (night_k() > 0.05f && !overcast()) {  // twinkle
      for (int i = 0; i < STAR_COUNT; i++) {
        int x, y;
        star_pos(i, x, y);
        invalidate_rect(back_obj, x - S(3), y - S(3), S(6), S(6));
      }
    }
  }

  if (weather == WX_FOG) {
    fog_x += Sf(6) * dt;
    invalidate_rect(front_obj, 0, S(110), W, S(150));
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

  // Each drop invalidates its box at the old and new position. Rain moves every
  // frame: the frame timer already ticks once per rendered frame, and stepping
  // less often made drops judder when the frame rate sat near the step rate.
  float step = dt;
  bool snow = weather == WX_SNOW;
  for (int i = 0; i < drop_count; i++) {
    Drop &d = drops[i];
    lv_area_t before = drop_bounds(d, snow);
    d.y += d.speed * step;
    if (!snow) d.x -= d.speed * step * 0.08f;
    if (d.y > H) {
      d.y = -10;
      d.x = frand() * (W + 20);
    }
    lv_area_t after = drop_bounds(d, snow);
    if (_lv_area_is_on(&before, &after)) {
      _lv_area_join(&after, &after, &before);
    } else {
      d2::invalidate(front_obj, before);
    }
    d2::invalidate(front_obj, after);
  }
}
