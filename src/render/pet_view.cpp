#include "pet_view.h"

#include <math.h>
#include <string.h>

#include "draw2d.h"
#include "icons.h"

#define MAX_PARTICLES 24

namespace {

enum PKind : uint8_t { P_NONE, P_HEART, P_SPARKLE, P_Z, P_CRUMB, P_SWEAT, P_TEAR, P_STINK, P_BANG };

struct Particle {
  uint8_t kind, size;
  float x, y, vx, vy;  // relative to the object's top-left
  float life, max_life;  // seconds
};

enum EyeStyle : uint8_t { E_NORMAL, E_HAPPY, E_CLOSED, E_X, E_BIG, E_SQUEEZE, E_HALF };
enum MouthStyle : uint8_t { M_SMILE, M_BIG, M_FROWN, M_FLAT, M_O, M_CHEW, M_WAVY };

struct Face {
  EyeStyle eyes;
  MouthStyle mouth;
  bool sad_brows;
};

struct PetView {
  int size;
  uint8_t form, palette, stage;
  Mood mood;
  uint8_t acc;
  float egg_progress;
  Food food;
  bool mirror;

  PetAnim anim;
  float anim_t, anim_dur;
  float t;

  float blink_timer, blink_t;
  float look_x, look_y, look_tx, look_ty, look_wander;
  bool look_active;
  int look_sx, look_sy;

  float lean, lean_target;
  float squash, squash_v;
  float jump, prev_jump;
  float idle_hop_t;
  float hop_timer;
  float emit_timer;

  Particle parts[MAX_PARTICLES];
  uint32_t rng;
};

struct Body {
  float cx, cy, w, h, top, ground;
  float shift;  // horizontal offset applied to the head features from leaning
};

struct Palette {
  uint32_t light, main, belly, dark;
};

const Palette PALETTES[] = {
    {0xFFD2B3, 0xFF8F6B, 0xFFEADD, 0xD9644A},  // peach
    {0xC2F5DE, 0x4FCF9F, 0xEBFFF6, 0x2E9E74},  // mint
    {0xC8E7FF, 0x5AA9F5, 0xEDF7FF, 0x3479C4},  // sky
    {0xE8D4FF, 0xA57BF0, 0xF6EFFF, 0x7451C2},  // lilac
    {0xFFF4B0, 0xFFC93D, 0xFFFBE3, 0xD99A1A},  // lemon
    {0xFFD0E2, 0xFF7AAA, 0xFFEBF3, 0xD14D7E},  // rose
};
const int PALETTE_N = sizeof(PALETTES) / sizeof(PALETTES[0]);

const uint32_t COL_EYE = 0x2B2440;
const uint32_t COL_MOUTH = 0x5A2A3A;
const uint32_t COL_TONGUE = 0xFF8FA3;
const uint32_t COL_CHEEK = 0xFF7C9C;

// Body aspect per form (width, height multipliers).
const float FORM_W[FORM_COUNT] = {1.0f, 1.05f, 0.95f, 0.92f, 1.0f, 1.28f, 0.9f, 1.0f, 1.02f, 1.0f};
const float FORM_H[FORM_COUNT] = {1.0f, 0.88f, 1.0f, 1.0f, 1.0f, 0.78f, 1.12f, 1.0f, 1.05f, 0.95f};
const float STAGE_SCALE[] = {0.8f, 0.72f, 0.84f, 0.93f, 1.0f};

inline lv_color_t hex(uint32_t c) { return lv_color_hex(c); }
inline const Palette &pal(const PetView &p) { return PALETTES[p.palette % PALETTE_N]; }

float frand(PetView &p) {
  p.rng = p.rng * 1664525u + 1013904223u;
  return (p.rng >> 8) / 16777216.0f;
}

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

PetView *get(lv_obj_t *obj) { return (PetView *)lv_obj_get_user_data(obj); }

float anim_duration(PetAnim a) {
  switch (a) {
    case ANIM_HAPPY: return 1.4f;
    case ANIM_PAT: return 0.7f;
    case ANIM_EAT: return 2.6f;
    case ANIM_REFUSE: return 1.0f;
    case ANIM_DIZZY: return 2.2f;
    case ANIM_SURPRISED: return 1.0f;
    case ANIM_DANCE: return 3.0f;
    case ANIM_MEDICINE: return 1.4f;
    case ANIM_HATCH: return 1.6f;
    case ANIM_WAKE: return 1.2f;
    default: return 0;
  }
}

void emit(PetView &p, PKind kind, float x, float y, float vx, float vy, float life, uint8_t size) {
  for (auto &q : p.parts) {
    if (q.kind == P_NONE) {
      q = {kind, size, x, y, vx, vy, life, life};
      return;
    }
  }
}

Body body_geom(const PetView &p, const lv_area_t &a) {
  Body b;
  float sc = STAGE_SCALE[p.stage <= STAGE_ADULT ? p.stage : 0];
  uint8_t f = p.form < FORM_COUNT ? p.form : FORM_BLOB;
  float base_w = p.size * sc * FORM_W[f];
  float base_h = p.size * sc * 0.88f * FORM_H[f];
  b.w = base_w * p.squash;
  b.h = base_h / p.squash;
  b.ground = a.y2 - 8;
  float shake = 0;
  if (p.acc & ACC_SCARF) shake += sinf(p.t * 45) * 0.7f;
  if (p.mood == MOOD_SICK && !p.anim) shake += sinf(p.t * 30) * 0.5f;
  if (p.anim == ANIM_HATCH) shake += sinf(p.t * 60) * 2.5f * (1 - p.anim_t / p.anim_dur);
  float hover = p.form == FORM_GHOST ? -6 - 3 * sinf(p.t * 2.2f) : 0;
  b.cx = (a.x1 + a.x2) / 2.0f + shake + p.lean * 6;
  b.cy = b.ground - b.h / 2 - 2 + p.jump + hover;
  b.top = b.cy - b.h / 2;
  b.shift = p.lean * b.w * 0.12f;
  return b;
}

Face pick_face(const PetView &p) {
  switch (p.anim) {
    case ANIM_HAPPY:
    case ANIM_PAT:
    case ANIM_DANCE:
    case ANIM_HATCH: return {E_HAPPY, M_BIG, false};
    case ANIM_EAT: return {E_HAPPY, M_CHEW, false};
    case ANIM_REFUSE: return {E_SQUEEZE, M_FROWN, false};
    case ANIM_DIZZY: return {E_X, M_WAVY, false};
    case ANIM_SURPRISED:
    case ANIM_WAKE: return {E_BIG, M_O, false};
    case ANIM_MEDICINE: return {E_SQUEEZE, M_WAVY, false};
    default: break;
  }
  switch (p.mood) {
    case MOOD_SLEEPING: return {E_CLOSED, M_FLAT, false};
    case MOOD_SICK: return {E_HALF, M_WAVY, true};
    case MOOD_HUNGRY: return {E_NORMAL, M_O, true};
    case MOOD_DIRTY: return {E_NORMAL, M_FLAT, false};
    case MOOD_SAD: return {E_NORMAL, M_FROWN, true};
    case MOOD_TIRED: return {E_HALF, M_FLAT, false};
    case MOOD_HAPPY: return {E_NORMAL, M_BIG, false};
    default: return {E_NORMAL, M_SMILE, false};
  }
}

// ---------------------------------------------------------------- drawing

void draw_egg(lv_draw_ctx_t *ctx, const PetView &p, const lv_area_t &a) {
  float rx = p.size * 0.36f, ry = p.size * 0.46f;
  float wob = 0;
  float burst = fmodf(p.t, 2.2f);
  float amp = p.egg_progress > 0.85f ? 4 : p.egg_progress > 0.5f ? 2 : 0.8f;
  if (burst < 0.5f) wob = sinf(burst * 30) * amp;
  wob += p.lean * 5;
  float cx = (a.x1 + a.x2) / 2.0f + wob * 0.5f;
  float ground = a.y2 - 8;
  float cy = ground - ry;

  d2::oval(ctx, cx, ground, rx * 0.9f, 4, lv_color_black(), LV_OPA_30);
  // Narrower top: a slightly offset upper oval over a wider base.
  d2::grad_rect(ctx, cx - rx + wob * 0.4f, cy - ry, rx * 2, ry * 1.4f, LV_RADIUS_CIRCLE, hex(0xFFFBF3), hex(0xF5E6CF));
  d2::grad_rect(ctx, cx - rx * 1.06f, cy - ry * 0.35f, rx * 2.12f, ry * 1.35f, LV_RADIUS_CIRCLE, hex(0xF8ECDA),
                hex(0xEBD5B8));
  lv_color_t spot = hex(pal(p).main);
  d2::oval(ctx, cx - rx * 0.45f, cy - ry * 0.15f, rx * 0.28f, ry * 0.2f, spot, LV_OPA_80);
  d2::oval(ctx, cx + rx * 0.4f, cy + ry * 0.25f, rx * 0.22f, ry * 0.16f, spot, LV_OPA_80);
  d2::oval(ctx, cx + rx * 0.05f, cy - ry * 0.62f, rx * 0.16f, ry * 0.12f, spot, LV_OPA_80);
  d2::oval(ctx, cx - rx * 0.1f, cy + ry * 0.62f, rx * 0.2f, ry * 0.12f, spot, LV_OPA_80);
  d2::oval(ctx, cx - rx * 0.45f, cy - ry * 0.55f, rx * 0.16f, ry * 0.22f, lv_color_white(), LV_OPA_70);

  if (p.egg_progress > 0.55f) {
    float k = clampf((p.egg_progress - 0.55f) / 0.45f, 0, 1);
    lv_color_t crack = hex(0x6B5A4A);
    float y0 = cy - ry * 0.1f;
    float pts[][2] = {{-0.9f, 0}, {-0.55f, -0.18f}, {-0.25f, 0.1f}, {0.05f, -0.2f}, {0.35f, 0.08f}, {0.65f, -0.15f}, {0.9f, 0.02f}};
    int n = 1 + (int)(k * 6);
    for (int i = 0; i < n && i < 6; i++)
      d2::line(ctx, cx + pts[i][0] * rx, y0 + pts[i][1] * ry, cx + pts[i + 1][0] * rx, y0 + pts[i + 1][1] * ry, 2, crack);
  }
}

void draw_ears_back(lv_draw_ctx_t *ctx, const PetView &p, const Body &b) {
  const Palette &c = pal(p);
  float t = p.t;
  bool droop = p.mood == MOOD_SAD || p.mood == MOOD_SICK || p.mood == MOOD_SLEEPING;

  switch (p.form) {
    case FORM_BUN:
      for (int s = -1; s <= 1; s += 2) {
        float bx = b.cx + b.shift + s * b.w * 0.2f, by = b.top + b.h * 0.12f;
        float tx = b.cx + b.shift * 1.6f + s * b.w * (droop ? 0.55f : 0.3f);
        float ty = b.top - b.h * (droop ? 0.1f : 0.55f) + sinf(t * 3 + s) * 1.5f;
        d2::line(ctx, bx, by, tx, ty, b.w * 0.2f, hex(c.main));
        d2::line(ctx, bx + (tx - bx) * 0.2f, by + (ty - by) * 0.2f, tx + (bx - tx) * 0.12f, ty + (by - ty) * 0.12f,
                 b.w * 0.08f, hex(COL_CHEEK), LV_OPA_60);
      }
      break;
    case FORM_KITTY:
    case FORM_DRAKE: {
      // Tail behind the body, swaying.
      int side = p.mirror ? -1 : 1;
      float sway = sinf(t * 2.5f) * 12;
      float tcx = b.cx + side * b.w * 0.42f, tcy = b.cy + b.h * 0.05f;
      int a0 = side > 0 ? 300 + (int)sway : 120 + (int)sway, a1 = side > 0 ? 70 + (int)sway : 240 + (int)sway;
      d2::arc(ctx, tcx, tcy, b.h * 0.32f, a0, a1, p.form == FORM_DRAKE ? 7 : 5, hex(c.dark));
      if (p.form == FORM_DRAKE) {
        // Wings.
        float flap = sinf(t * (p.anim == ANIM_DANCE ? 14 : 3)) * 5;
        for (int s = -1; s <= 1; s += 2) {
          d2::tri(ctx, b.cx + s * b.w * 0.3f, b.cy - b.h * 0.15f, b.cx + s * b.w * 0.82f, b.cy - b.h * 0.42f - flap,
                  b.cx + s * b.w * 0.5f, b.cy + b.h * 0.18f, hex(c.dark), LV_OPA_90);
          d2::tri(ctx, b.cx + s * b.w * 0.36f, b.cy - b.h * 0.08f, b.cx + s * b.w * 0.72f, b.cy - b.h * 0.3f - flap,
                  b.cx + s * b.w * 0.5f, b.cy + b.h * 0.08f, hex(c.light), LV_OPA_50);
        }
      }
      break;
    }
    case FORM_JELLY:
      for (int k = -2; k <= 2; k++) {
        if (k == 0) continue;
        float x0 = b.cx + k * b.w * 0.15f;
        float x1 = x0 + sinf(t * 3 + k) * 4 + k * 2;
        d2::line(ctx, x0, b.cy + b.h * 0.2f, x1, b.ground + 1, 6, hex(c.main), LV_OPA_90);
      }
      break;
    default: break;
  }
}

void draw_feet(lv_draw_ctx_t *ctx, const PetView &p, const Body &b) {
  if (p.form == FORM_GHOST || p.form == FORM_JELLY) return;
  lv_color_t c = hex(pal(p).dark);
  float fy = b.cy + b.h * 0.46f;
  float step = (p.anim == ANIM_DANCE) ? sinf(p.t * 10) * 2 : 0;
  d2::oval(ctx, b.cx - b.w * 0.22f, fy - step, b.w * 0.14f, b.h * 0.08f + 1, c);
  d2::oval(ctx, b.cx + b.w * 0.22f, fy + step, b.w * 0.14f, b.h * 0.08f + 1, c);
}

void draw_body(lv_draw_ctx_t *ctx, const PetView &p, const Body &b) {
  const Palette &c = pal(p);
  lv_opa_t opa = p.form == FORM_GHOST ? 225 : LV_OPA_COVER;
  float radius = LV_MIN(b.w, b.h) * (p.form == FORM_MOCHI ? 0.5f : 0.46f);
  float x = b.cx - b.w / 2, y = b.top;

  if (p.form == FORM_GHOST) {
    d2::grad_rect(ctx, x, y, b.w, b.h * 0.92f, radius, hex(c.light), hex(c.main), opa);
    for (int k = 0; k < 4; k++) {
      float sx = x + b.w * (0.125f + k * 0.25f);
      d2::circle(ctx, sx, y + b.h * 0.86f + sinf(p.t * 4 + k) * 1.5f, b.w * 0.13f, hex(c.main), opa);
    }
  } else {
    d2::grad_rect(ctx, x, y, b.w, b.h, radius, hex(c.light), hex(c.main), opa);
  }

  d2::oval(ctx, b.cx + b.shift * 0.4f, b.cy + b.h * 0.2f, b.w * 0.3f, b.h * 0.22f, hex(c.belly), LV_OPA_70);
  d2::oval(ctx, b.cx - b.w * 0.24f + b.shift * 0.5f, b.top + b.h * 0.2f, b.w * 0.11f, b.h * 0.07f, lv_color_white(),
           LV_OPA_60);
  if (p.form == FORM_JELLY)
    d2::oval(ctx, b.cx + b.w * 0.18f, b.top + b.h * 0.16f, b.w * 0.05f, b.h * 0.04f, lv_color_white(), LV_OPA_70);
  if (p.mood == MOOD_SICK)
    d2::grad_rect(ctx, x + 2, y + 2, b.w - 4, b.h * 0.55f, radius, hex(0x9BD47A), hex(c.main), LV_OPA_40);
}

void draw_head_front(lv_draw_ctx_t *ctx, const PetView &p, const Body &b) {
  const Palette &c = pal(p);
  float t = p.t;
  float hx = b.cx + b.shift;
  switch (p.form) {
    case FORM_BLOB:
      d2::arc(ctx, hx + 3, b.top - 3, 5, 150, 30, 3, hex(c.dark));
      break;
    case FORM_KITTY:
      for (int s = -1; s <= 1; s += 2) {
        float droop = (p.mood == MOOD_SAD || p.mood == MOOD_SLEEPING) ? b.h * 0.08f : 0;
        d2::tri(ctx, hx + s * b.w * 0.08f, b.top + b.h * 0.06f, hx + s * b.w * 0.44f, b.top + b.h * 0.24f,
                hx + s * b.w * 0.36f, b.top - b.h * 0.22f + droop, hex(c.main));
        d2::tri(ctx, hx + s * b.w * 0.17f, b.top + b.h * 0.08f, hx + s * b.w * 0.37f, b.top + b.h * 0.18f,
                hx + s * b.w * 0.33f, b.top - b.h * 0.1f + droop, hex(COL_CHEEK), LV_OPA_60);
      }
      break;
    case FORM_SPROUT: {
      float sway = sinf(t * 1.8f) * 3;
      float tx = hx + sway, ty = b.top - 13;
      lv_color_t g1 = hex(0x5CC46A), g2 = hex(0x8FE08F);
      d2::line(ctx, hx, b.top + 3, tx, ty, 3, g1);
      d2::line(ctx, tx, ty, tx - 11 + sway * 0.3f, ty - 4, 7, g1);
      d2::line(ctx, tx, ty, tx + 10 + sway * 0.3f, ty - 7, 7, g2);
      break;
    }
    case FORM_MOCHI:
      for (int k = 0; k < 3; k++)
        d2::circle(ctx, hx - b.w * 0.15f + k * b.w * 0.15f, b.top + b.h * 0.1f + (k == 1 ? -2 : 0), 1.6f,
                   lv_color_white(), LV_OPA_80);
      d2::oval(ctx, hx + b.w * 0.3f, b.top + b.h * 0.08f, 6, 4, hex(0xFF9EC0));
      d2::oval(ctx, hx + b.w * 0.36f, b.top + b.h * 0.02f, 5, 3, hex(0x8FE08F));
      break;
    case FORM_GHOST:
      d2::arc(ctx, hx - 2, b.top - 2, 5, 200, 20, 2, hex(c.light), LV_OPA_80);
      break;
    case FORM_STAR:
      for (int s = -1; s <= 1; s += 2) {
        float ax = hx + s * b.w * 0.24f + sinf(t * 2 + s) * 2, ay = b.top - 14;
        d2::line(ctx, hx + s * b.w * 0.12f, b.top + 4, ax, ay, 2, hex(c.dark));
        float glow = 6 + sinf(t * 4 + s) * 1.5f;
        d2::circle(ctx, ax, ay, glow, hex(0xFFE066), LV_OPA_30);
        d2::circle(ctx, ax, ay, 3.5f, hex(0xFFE066));
      }
      break;
    case FORM_DRAKE:
      for (int s = -1; s <= 1; s += 2)
        d2::tri(ctx, hx + s * b.w * 0.12f, b.top + 5, hx + s * b.w * 0.3f, b.top + 3, hx + s * b.w * 0.26f, b.top - 11,
                hex(0xFFF3D6));
      break;
    default: break;
  }
}

void draw_eyes(lv_draw_ctx_t *ctx, const PetView &p, const Body &b, const Face &f, float eye_open) {
  lv_color_t ink = hex(COL_EYE);
  float rx = LV_MAX(3.0f, b.w * 0.075f), ry = LV_MAX(4.0f, b.h * 0.13f);
  float ey = b.cy - b.h * 0.07f;
  float lx = p.look_x * 2.6f, ly = p.look_y * 2.2f;
  float lw = LV_MAX(2.0f, b.w * 0.035f);

  for (int s = -1; s <= 1; s += 2) {
    float ex = b.cx + b.shift + s * b.w * 0.2f;
    switch (f.eyes) {
      case E_HAPPY: d2::arc(ctx, ex, ey + ry * 0.45f, rx * 1.15f, 205, 335, lw, ink); break;
      case E_CLOSED: d2::arc(ctx, ex, ey - ry * 0.25f, rx * 1.1f, 25, 155, lw, ink); break;
      case E_X: {
        float spin = p.t * 8 + s;
        float dx = cosf(spin) * rx, dy = sinf(spin) * rx;
        d2::line(ctx, ex - dx, ey - dy, ex + dx, ey + dy, lw, ink);
        d2::line(ctx, ex + dy, ey - dx, ex - dy, ey + dx, lw, ink);
        break;
      }
      case E_SQUEEZE: {
        float d = s < 0 ? 1 : -1;  // '>' on the left, '<' on the right
        d2::line(ctx, ex - d * rx, ey - ry * 0.6f, ex + d * rx, ey, lw, ink);
        d2::line(ctx, ex + d * rx, ey, ex - d * rx, ey + ry * 0.6f, lw, ink);
        break;
      }
      default: {
        float k = f.eyes == E_BIG ? 1.25f : 1.0f;
        float open = eye_open * (f.eyes == E_HALF ? 0.55f : 1.0f);
        float erx = rx * k, ery = ry * k * open;
        if (ery < 1.6f) {
          d2::line(ctx, ex - erx, ey, ex + erx, ey, lw, ink);
          break;
        }
        float cx = ex + lx, cy = ey + ly + (f.eyes == E_HALF ? ry * 0.3f : 0);
        d2::oval(ctx, cx, cy, erx, ery, ink);
        if (ery > 3) {
          d2::circle(ctx, cx - erx * 0.3f, cy - ery * 0.4f, erx * 0.45f, lv_color_white());
          d2::circle(ctx, cx + erx * 0.35f, cy + ery * 0.35f, erx * 0.2f, lv_color_white(), LV_OPA_80);
        }
        break;
      }
    }
    if (f.sad_brows) {
      float d = -s;  // inner end is towards the centre
      d2::line(ctx, ex - d * rx * 1.1f, ey - ry - 2, ex + d * rx * 0.9f, ey - ry - 5, LV_MAX(1.5f, lw * 0.7f), ink,
               LV_OPA_80);
    }
    if (p.mood != MOOD_SICK)
      d2::oval(ctx, ex + s * rx * 0.5f, ey + ry + rx * 0.4f, rx * 0.95f, rx * 0.5f, hex(COL_CHEEK), LV_OPA_40);
  }

  if (p.form == FORM_KITTY) {
    lv_color_t wh = hex(COL_EYE);
    float my = b.cy + b.h * 0.1f;
    for (int s = -1; s <= 1; s += 2) {
      float x0 = b.cx + b.shift + s * b.w * 0.3f;
      d2::line(ctx, x0, my, x0 + s * b.w * 0.22f, my - 2, 1, wh, LV_OPA_50);
      d2::line(ctx, x0, my + 3, x0 + s * b.w * 0.22f, my + 5, 1, wh, LV_OPA_50);
    }
  }
}

void draw_mouth(lv_draw_ctx_t *ctx, const PetView &p, const Body &b, const Face &f) {
  lv_color_t ink = hex(COL_MOUTH);
  float mx = b.cx + b.shift * 1.1f, my = b.cy + b.h * 0.1f;
  float r = LV_MAX(3.0f, b.w * 0.075f);
  float lw = LV_MAX(2.0f, b.w * 0.03f);
  switch (f.mouth) {
    case M_SMILE: d2::arc(ctx, mx, my - r * 0.5f, r, 30, 150, lw, ink); break;
    case M_BIG:
      d2::arc(ctx, mx, my - 1, r * 1.1f, 0, 180, r * 1.1f, ink);
      d2::oval(ctx, mx, my + r * 0.62f, r * 0.5f, r * 0.28f, hex(COL_TONGUE));
      break;
    case M_FROWN: d2::arc(ctx, mx, my + r * 0.8f, r * 0.85f, 210, 330, lw, ink); break;
    case M_FLAT: d2::line(ctx, mx - r * 0.55f, my + 1, mx + r * 0.55f, my + 1, lw, ink); break;
    case M_O: d2::oval(ctx, mx, my + 2, r * 0.42f, r * 0.55f, ink); break;
    case M_CHEW: {
      float open = fabsf(sinf(p.anim_t * 9));
      if (open > 0.3f)
        d2::oval(ctx, mx, my + 2, r * 0.6f, LV_MAX(1.5f, r * 0.7f * open), ink);
      else
        d2::arc(ctx, mx, my - r * 0.5f, r * 0.8f, 30, 150, lw, ink);
      break;
    }
    case M_WAVY: {
      float w = r * 0.6f;
      for (int i = 0; i < 4; i++)
        d2::line(ctx, mx - w * 2 + i * w, my + ((i & 1) ? 2 : 0), mx - w + i * w, my + ((i & 1) ? 0 : 2),
                 LV_MAX(1.5f, lw * 0.8f), ink);
      break;
    }
  }
}

void draw_accessories(lv_draw_ctx_t *ctx, const PetView &p, const Body &b) {
  float hx = b.cx + b.shift;
  if (p.acc & ACC_SCARF) {
    float ny = b.cy + b.h * 0.25f;
    d2::line(ctx, b.cx - b.w * 0.4f, ny, b.cx + b.w * 0.4f, ny + 1, 7, hex(0xE84A5F));
    d2::line(ctx, b.cx + b.w * 0.22f, ny, b.cx + b.w * 0.3f + sinf(p.t * 3) * 2, ny + 13, 6, hex(0xC93A4E));
  }
  if ((p.acc & ACC_SUNGLASSES) && p.mood != MOOD_SLEEPING) {
    float ey = b.cy - b.h * 0.07f;
    float gw = b.w * 0.2f, gh = b.h * 0.17f;
    lv_color_t g = hex(0x1E1E2A);
    for (int s = -1; s <= 1; s += 2) {
      float ex = hx + s * b.w * 0.2f;
      d2::rect(ctx, ex - gw / 2 - 1, ey - gh / 2, gw + 2, gh, 4, g, 240);
      d2::line(ctx, ex - gw * 0.3f, ey - gh * 0.15f, ex - gw * 0.05f, ey - gh * 0.3f, 2, lv_color_white(), LV_OPA_50);
    }
    d2::line(ctx, hx - b.w * 0.1f, ey - gh * 0.2f, hx + b.w * 0.1f, ey - gh * 0.2f, 2, g);
  }
  if (p.acc & ACC_UMBRELLA) {
    float R = b.w * 0.68f;
    float ux = hx + b.w * 0.05f, uy = b.top - 14 + sinf(p.t * 1.5f) * 1.5f;
    d2::line(ctx, ux, uy, ux + b.w * 0.18f, b.cy + b.h * 0.05f, 2, hex(0x4A4458));
    d2::arc(ctx, ux + b.w * 0.18f - 3, b.cy + b.h * 0.05f, 3, 0, 180, 2, hex(0x4A4458));
    for (int i = 0; i < 4; i++)
      d2::arc(ctx, ux, uy, R, 180 + i * 45, 225 + i * 45, R, hex((i & 1) ? 0xFFD1DC : 0xFF6B8B));
    d2::circle(ctx, ux, uy - R, 2.5f, hex(0x4A4458));
  }
}

void draw_food(lv_draw_ctx_t *ctx, const PetView &p, const Body &b) {
  if (p.anim != ANIM_EAT) return;
  float k = 1 - clampf(p.anim_t / (p.anim_dur * 0.85f), 0, 1);
  if (k <= 0.05f) return;
  static const Icon icons[FOOD_COUNT] = {ICON_FOOD, ICON_FRUIT, ICON_CANDY, ICON_CAKE};
  static const uint32_t colors[FOOD_COUNT] = {0xE8A15A, 0xFF5A5A, 0xFF9AD5, 0xFFB3C7};
  int sz = (int)(b.w * 0.42f * (0.4f + 0.6f * k));
  icon_draw(ctx, icons[p.food % FOOD_COUNT], b.cx + b.shift, b.cy + b.h * 0.38f, sz, hex(colors[p.food % FOOD_COUNT]));
}

void draw_effects(lv_draw_ctx_t *ctx, const PetView &p, const Body &b) {
  if (p.anim == ANIM_DIZZY) {
    for (int i = 0; i < 3; i++) {
      float a = p.t * 5 + i * 2.094f;
      icon_draw(ctx, ICON_CLEAN, b.cx + cosf(a) * b.w * 0.38f, b.top - 4 + sinf(a) * 5, 10, hex(0xFFE066));
    }
  }
  if (p.anim == ANIM_HATCH) {
    float k = p.anim_t / p.anim_dur;
    d2::circle(ctx, b.cx, b.cy, b.w * (0.3f + k * 0.9f), lv_color_white(), (lv_opa_t)(220 * (1 - k)));
  }
}

void draw_particles(lv_draw_ctx_t *ctx, const PetView &p, const lv_area_t &a) {
  for (const auto &q : p.parts) {
    if (q.kind == P_NONE) continue;
    float k = q.life / q.max_life;  // 1 → 0
    float x = a.x1 + q.x, y = a.y1 + q.y;
    lv_opa_t opa = (lv_opa_t)(255 * clampf(k * 2.5f, 0, 1));
    int sz = (int)(q.size * (k < 0.3f ? 0.5f + k * 1.6f : 1.0f));
    switch (q.kind) {
      case P_HEART: icon_draw(ctx, ICON_HEART, x, y, sz, hex(0xFF5C8A)); break;
      case P_SPARKLE: icon_draw(ctx, ICON_CLEAN, x, y, sz, hex(0xFFE066)); break;
      case P_Z: d2::text(ctx, x - 10, y - 8, 20, q.size > 12 ? "Z" : "z", q.size > 12 ? &lv_font_montserrat_20 : &lv_font_montserrat_14, lv_color_white(), LV_TEXT_ALIGN_CENTER, opa); break;
      case P_CRUMB: d2::circle(ctx, x, y, 2, hex(0xE0A060), opa); break;
      case P_SWEAT:
      case P_TEAR: icon_draw(ctx, ICON_DROP, x, y, sz, hex(0x8FD3FF)); break;
      case P_STINK: d2::arc(ctx, x, y, 4, 200, 340, 2, hex(0x9AA07A), opa); d2::arc(ctx, x + 6, y - 2, 4, 20, 160, 2, hex(0x9AA07A), opa); break;
      case P_BANG: d2::text(ctx, x - 10, y - 10, 20, "!", &lv_font_montserrat_28, hex(0xFFE066), LV_TEXT_ALIGN_CENTER, opa); break;
    }
  }
}

void draw_cb(lv_event_t *e) {
  lv_obj_t *obj = lv_event_get_target(e);
  PetView *pv = get(obj);
  if (!pv) return;
  lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
  lv_area_t a;
  lv_obj_get_coords(obj, &a);

  if (pv->form == FORM_EGG) {
    draw_egg(ctx, *pv, a);
    draw_particles(ctx, *pv, a);
    return;
  }

  Body b = body_geom(*pv, a);
  Face f = pick_face(*pv);
  float eye_open = 1;
  if (pv->blink_t >= 0) eye_open = fabsf(pv->blink_t / 0.08f - 1);

  float shadow_k = clampf(1 + (b.cy + b.h / 2 - b.ground) / 40, 0.5f, 1);
  d2::oval(ctx, b.cx, b.ground, b.w * 0.42f * shadow_k, 3.5f, lv_color_black(), (lv_opa_t)(70 * shadow_k));

  draw_ears_back(ctx, *pv, b);
  draw_feet(ctx, *pv, b);
  draw_body(ctx, *pv, b);
  draw_head_front(ctx, *pv, b);
  draw_eyes(ctx, *pv, b, f, eye_open);
  draw_mouth(ctx, *pv, b, f);
  draw_accessories(ctx, *pv, b);
  draw_food(ctx, *pv, b);
  draw_effects(ctx, *pv, b);
  draw_particles(ctx, *pv, a);
}

void delete_cb(lv_event_t *e) {
  lv_obj_t *obj = lv_event_get_target(e);
  delete get(obj);
  lv_obj_set_user_data(obj, nullptr);
}

// ---------------------------------------------------------------- animation

float hop(float t, float dur, float height) {
  if (t < 0 || t > dur) return 0;
  return -sinf((float)M_PI * t / dur) * height;
}

void update_anim(PetView &p, float dt, int w, int h) {
  float jump = 0;
  float lean_anim = 0;
  float squash_target = 1;

  float period = p.mood == MOOD_SLEEPING ? 4.0f : 2.6f;
  squash_target += 0.03f * sinf(2 * (float)M_PI * p.t / period);
  if (p.mood == MOOD_SAD || p.mood == MOOD_TIRED) squash_target += 0.04f;

  if (p.anim) {
    p.anim_t += dt;
    float t = p.anim_t;
    switch (p.anim) {
      case ANIM_HAPPY: jump = hop(t, 0.45f, 16) + hop(t - 0.6f, 0.45f, 12); break;
      case ANIM_PAT: squash_target += 0.12f * sinf((float)M_PI * clampf(t / 0.4f, 0, 1)); break;
      case ANIM_EAT: squash_target += 0.04f * fabsf(sinf(t * 9)); break;
      case ANIM_REFUSE: lean_anim = sinf(t * 22) * 0.7f * (1 - t / p.anim_dur); break;
      case ANIM_DIZZY: lean_anim = sinf(t * 5) * 0.5f; break;
      case ANIM_SURPRISED:
      case ANIM_WAKE: jump = hop(t, 0.35f, 14); break;
      case ANIM_DANCE:
        jump = -fabsf(sinf(t * 7)) * 9;
        lean_anim = sinf(t * 3.5f) * 0.8f;
        break;
      case ANIM_MEDICINE: lean_anim = sinf(t * 30) * 0.25f; squash_target += 0.08f; break;
      case ANIM_HATCH: squash_target += 0.1f * sinf(t * 20) * (1 - t / p.anim_dur); break;
      default: break;
    }
    if (p.anim == ANIM_EAT) {
      float cycle = fmodf(t, 0.7f);
      if (cycle < dt && t < p.anim_dur * 0.85f)
        for (int i = 0; i < 2; i++)
          emit(p, P_CRUMB, w / 2 + (frand(p) - 0.5f) * 20, h * 0.72f, (frand(p) - 0.5f) * 50, -30 - frand(p) * 30, 0.6f, 3);
    }
    if (p.anim_t >= p.anim_dur) p.anim = ANIM_NONE;
  } else if (p.mood == MOOD_HAPPY || p.mood == MOOD_CONTENT) {
    // Occasional idle hop.
    p.hop_timer -= dt;
    if (p.hop_timer <= 0) {
      p.hop_timer = 5 + frand(p) * 7;
      p.idle_hop_t = 0;
    }
  }
  if (p.idle_hop_t >= 0) {
    p.idle_hop_t += dt;
    jump += hop(p.idle_hop_t, 0.35f, p.mood == MOOD_HAPPY ? 10 : 6);
    if (p.idle_hop_t > 0.35f) p.idle_hop_t = -1;
  }

  // Squash on landing.
  if (p.prev_jump < -3 && jump > -1) p.squash_v += 2.2f;
  p.prev_jump = jump;
  p.jump = jump;
  float acc = (squash_target - p.squash) * 220 - p.squash_v * 14;
  p.squash_v += acc * dt;
  p.squash += p.squash_v * dt;
  p.squash = clampf(p.squash, 0.75f, 1.3f);

  float lean_goal = clampf(p.lean_target + lean_anim, -1, 1);
  p.lean += (lean_goal - p.lean) * clampf(dt * 8, 0, 1);

  // Blinking.
  if (p.mood != MOOD_SLEEPING) {
    p.blink_timer -= dt;
    if (p.blink_timer <= 0) {
      p.blink_timer = 2 + frand(p) * 3.5f;
      p.blink_t = 0;
    }
  }
  if (p.blink_t >= 0) {
    p.blink_t += dt;
    if (p.blink_t > 0.16f) p.blink_t = -1;
  }

  // Gaze: follow the touch point, otherwise glance around now and then.
  if (!p.look_active) {
    p.look_wander -= dt;
    if (p.look_wander <= 0) {
      p.look_wander = 1.5f + frand(p) * 3;
      bool centre = frand(p) < 0.4f;
      p.look_tx = centre ? 0 : (frand(p) - 0.5f) * 1.4f;
      p.look_ty = centre ? 0 : (frand(p) - 0.5f) * 0.8f;
    }
  }
  p.look_x += (p.look_tx - p.look_x) * clampf(dt * 10, 0, 1);
  p.look_y += (p.look_ty - p.look_y) * clampf(dt * 10, 0, 1);

  // Mood ambience particles.
  p.emit_timer -= dt;
  if (p.emit_timer <= 0 && p.form != FORM_EGG) {
    float cx = w / 2.0f, top = h * 0.35f;
    switch (p.mood) {
      case MOOD_SLEEPING:
        emit(p, P_Z, cx + 18, top, 12, -18, 2.2f, frand(p) < 0.5f ? 14 : 10);
        p.emit_timer = 1.6f;
        break;
      case MOOD_SICK:
        emit(p, P_SWEAT, cx + 22, top + 8, 4, 25, 1.0f, 9);
        p.emit_timer = 2.5f;
        break;
      case MOOD_SAD:
        emit(p, P_TEAR, cx - 12, h * 0.6f, -2, 30, 0.8f, 8);
        p.emit_timer = 3.0f;
        break;
      case MOOD_DIRTY:
        emit(p, P_STINK, cx + (frand(p) - 0.5f) * 40, top + 10, 0, -20, 1.4f, 8);
        p.emit_timer = 1.3f;
        break;
      default: p.emit_timer = 1; break;
    }
  }

  for (auto &q : p.parts) {
    if (q.kind == P_NONE) continue;
    q.life -= dt;
    if (q.life <= 0) {
      q.kind = P_NONE;
      continue;
    }
    q.x += q.vx * dt;
    q.y += q.vy * dt;
    if (q.kind == P_CRUMB) q.vy += 160 * dt;
    if (q.kind == P_HEART || q.kind == P_Z) q.x += sinf(q.life * 6) * 12 * dt;
  }
}

}  // namespace

// ---------------------------------------------------------------- public API

lv_obj_t *pet_view_create(lv_obj_t *parent, int size) {
  lv_obj_t *obj = lv_obj_create(parent);
  lv_obj_remove_style_all(obj);
  lv_obj_set_size(obj, size * 2, (int)(size * 2.1f));
  lv_obj_update_layout(obj);  // so callers can read the size straight away to position it
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);

  PetView *p = new PetView();
  memset(p, 0, sizeof(*p));
  p->size = size;
  p->form = FORM_BLOB;
  p->stage = STAGE_BABY;
  p->mood = MOOD_CONTENT;
  p->squash = 1;
  p->blink_t = -1;
  p->idle_hop_t = -1;
  p->blink_timer = 1.5f;
  p->hop_timer = 4;
  p->rng = (uint32_t)(uintptr_t)obj ^ lv_tick_get();
  lv_obj_set_user_data(obj, p);
  lv_obj_add_event_cb(obj, draw_cb, LV_EVENT_DRAW_MAIN, nullptr);
  lv_obj_add_event_cb(obj, delete_cb, LV_EVENT_DELETE, nullptr);
  return obj;
}

void pet_view_set_look(lv_obj_t *obj, uint8_t form, uint8_t palette, uint8_t stage) {
  PetView *p = get(obj);
  p->form = form < FORM_COUNT ? form : FORM_BLOB;
  p->palette = palette;
  p->stage = stage;
}

void pet_view_set_mood(lv_obj_t *obj, Mood mood) { get(obj)->mood = mood; }
void pet_view_set_accessories(lv_obj_t *obj, uint8_t acc) { get(obj)->acc = acc; }
void pet_view_set_egg_progress(lv_obj_t *obj, float progress) { get(obj)->egg_progress = progress; }
void pet_view_set_food(lv_obj_t *obj, Food food) { get(obj)->food = food; }
void pet_view_set_mirror(lv_obj_t *obj, bool facing_left) { get(obj)->mirror = facing_left; }
void pet_view_set_lean(lv_obj_t *obj, float lean) { get(obj)->lean_target = clampf(lean, -1, 1); }

void pet_view_play(lv_obj_t *obj, PetAnim anim) {
  PetView *p = get(obj);
  p->anim = anim;
  p->anim_t = 0;
  p->anim_dur = anim_duration(anim);
  int w = lv_obj_get_width(obj), h = lv_obj_get_height(obj);
  switch (anim) {
    case ANIM_HAPPY:
    case ANIM_DANCE: pet_view_emit_hearts(obj, 3); break;
    case ANIM_PAT: pet_view_emit_hearts(obj, 1); break;
    case ANIM_HATCH: pet_view_emit_sparkles(obj, 8); break;
    case ANIM_SURPRISED:
    case ANIM_WAKE: emit(*p, P_BANG, w / 2.0f + 24, h * 0.3f, 0, -10, 0.9f, 20); break;
    default: break;
  }
}

bool pet_view_busy(lv_obj_t *obj) { return get(obj)->anim != ANIM_NONE; }

void pet_view_look_at(lv_obj_t *obj, bool active, int x, int y) {
  PetView *p = get(obj);
  p->look_active = active;
  if (!active) return;
  lv_area_t a;
  lv_obj_get_coords(obj, &a);
  float cx = (a.x1 + a.x2) / 2.0f, cy = a.y1 + lv_area_get_height(&a) * 0.6f;
  p->look_tx = clampf((x - cx) / 50.0f, -1, 1);
  p->look_ty = clampf((y - cy) / 60.0f, -1, 1);
}

void pet_view_tick(lv_obj_t *obj, uint32_t dt_ms) {
  PetView *p = get(obj);
  float dt = clampf(dt_ms / 1000.0f, 0, 0.1f);
  p->t += dt;
  update_anim(*p, dt, lv_obj_get_width(obj), lv_obj_get_height(obj));
  lv_obj_invalidate(obj);
}

bool pet_view_hit(lv_obj_t *obj, int x, int y) {
  PetView *p = get(obj);
  lv_area_t a;
  lv_obj_get_coords(obj, &a);
  Body b = body_geom(*p, a);
  float m = 8;
  return x >= b.cx - b.w / 2 - m && x <= b.cx + b.w / 2 + m && y >= b.top - m && y <= b.ground + 2;
}

void pet_view_emit_hearts(lv_obj_t *obj, int count) {
  PetView *p = get(obj);
  int w = lv_obj_get_width(obj), h = lv_obj_get_height(obj);
  for (int i = 0; i < count; i++)
    emit(*p, P_HEART, w / 2.0f + (frand(*p) - 0.5f) * 50, h * 0.5f, (frand(*p) - 0.5f) * 20, -28 - frand(*p) * 14,
         1.3f + frand(*p) * 0.5f, 12 + (int)(frand(*p) * 6));
}

void pet_view_emit_sparkles(lv_obj_t *obj, int count) {
  PetView *p = get(obj);
  int w = lv_obj_get_width(obj), h = lv_obj_get_height(obj);
  for (int i = 0; i < count; i++) {
    float a = frand(*p) * 6.283f;
    float sp = 40 + frand(*p) * 50;
    emit(*p, P_SPARKLE, w / 2.0f, h * 0.6f, cosf(a) * sp, sinf(a) * sp - 20, 0.8f + frand(*p) * 0.4f,
         9 + (int)(frand(*p) * 6));
  }
}

lv_color_t pet_palette_main(uint8_t palette) { return lv_color_hex(PALETTES[palette % PALETTE_N].main); }
lv_color_t pet_palette_light(uint8_t palette) { return lv_color_hex(PALETTES[palette % PALETTE_N].light); }
