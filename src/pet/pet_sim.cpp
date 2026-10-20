#include "pet_sim.h"

#include <string.h>

const FoodInfo FOODS[FOOD_COUNT] = {
    // name     price hunger happy health weight
    {"Meal", 5, 35, 3, 2, 2},
    {"Fruit", 6, 20, 5, 6, 1},
    {"Snack", 8, 12, 15, 0, 2},
    {"Cake", 20, 18, 35, -2, 4},
};

static const char *const NAMES[] = {"Tomo", "Mochi", "Pip", "Nori", "Kiko", "Yuzu",
                                    "Bean", "Miso", "Lumi", "Taro", "Suki", "Bobo"};
static const int NAME_COUNT = sizeof(NAMES) / sizeof(NAMES[0]);
static const int PALETTE_COUNT = 6;

static const uint32_t STEP_S = 60;
static const uint32_t MAX_CATCHUP_S = 7 * 24 * 3600;

// Indexed by Stage.
static const uint32_t STAGE_DURATION_S[] = {300, 3600, 24 * 3600, 3 * 24 * 3600, 0};
static const float HUNGER_PER_H[] = {0, 12, 9, 7, 6};
static const float HAPPY_PER_H[] = {0, 8, 6, 5, 4};
static const float POOP_EVERY_H[] = {0, 2.0f, 3.0f, 3.5f, 4.0f};
static const uint8_t SLEEP_FROM[] = {0, 20, 21, 22, 22};
static const uint8_t SLEEP_UNTIL[] = {0, 8, 8, 7, 7};
static const float BASE_WEIGHT[] = {0, 5, 10, 18, 25};

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void clamp_stats(PetState &s) {
  s.hunger = clampf(s.hunger, 0, 100);
  s.happiness = clampf(s.happiness, 0, 100);
  s.energy = clampf(s.energy, 0, 100);
  s.hygiene = clampf(s.hygiene, 0, 100);
  s.health = clampf(s.health, 0, 100);
  if (s.weight < 1) s.weight = 1;
}

static bool in_sleep_window(uint8_t stage, int hour) {
  if (stage == STAGE_EGG) return false;
  return hour >= SLEEP_FROM[stage] || hour < SLEEP_UNTIL[stage];
}

void pet_init(PetState &s, uint32_t pet_id, uint32_t now_epoch, uint8_t generation) {
  memset(&s, 0, sizeof(s));
  s.pet_id = pet_id;
  const char *name = generation == 0 ? NAMES[0] : NAMES[pet_id % NAME_COUNT];
  strncpy(s.name, name, PET_NAME_LEN - 1);
  s.stage = STAGE_EGG;
  s.form = FORM_EGG;
  s.palette = pet_id % PALETTE_COUNT;
  s.generation = generation;
  s.hunger = s.happiness = s.energy = s.hygiene = s.health = 80;
  s.weight = 1;
  s.last_epoch = now_epoch;
  s.coins = 30;
  s.light_on = true;
}

static uint8_t next_form(const PetState &s) {
  uint16_t m = s.stage_mistakes;
  switch (s.stage) {
    case STAGE_EGG: return FORM_BLOB;
    case STAGE_BABY: return m <= 1 ? FORM_BUN : FORM_SPROUT;
    case STAGE_CHILD:
      if (s.form == FORM_BUN) return m <= 2 ? FORM_KITTY : FORM_MOCHI;
      return m <= 3 ? FORM_MOCHI : FORM_GHOST;
    case STAGE_TEEN:
      if (m <= 1) {
        if (s.form == FORM_KITTY) return FORM_DRAKE;
        if (s.form == FORM_MOCHI) return FORM_STAR;
        return FORM_JELLY;
      }
      return m <= 4 ? FORM_JELLY : FORM_GHOST;
    default: return s.form;
  }
}

static uint32_t evolve(PetState &s) {
  s.form = next_form(s);
  s.stage++;
  s.stage_s = 0;
  s.stage_mistakes = 0;
  s.weight = BASE_WEIGHT[s.stage];
  if (s.stage == STAGE_BABY) {
    s.hunger = 60;
    s.happiness = 70;
    s.energy = 90;
    s.hygiene = 100;
    s.health = 100;
    return EVT_HATCHED;
  }
  return EVT_EVOLVED;
}

static void care_mistake(PetState &s, uint32_t &events) {
  s.care_mistakes++;
  s.stage_mistakes++;
  events |= EVT_CARE_MISTAKE;
}

// Adds dt to a continuous-condition timer and reports whether it just crossed `threshold`.
static bool timer_crossed(uint32_t &timer, uint32_t dt, uint32_t threshold) {
  uint32_t before = timer;
  timer += dt;
  return before < threshold && timer >= threshold;
}

static uint32_t step(PetState &s, uint32_t dt, uint32_t epoch, int hour) {
  uint32_t events = 0;

  if (s.stage == STAGE_EGG) {
    s.stage_s += dt;
    if (s.stage_s >= STAGE_DURATION_S[STAGE_EGG]) events |= evolve(s);
    return events;
  }

  s.age_s += dt;
  s.stage_s += dt;
  float h = dt / 3600.0f;
  const uint8_t st = s.stage;

  // Sleep transitions.
  bool night = in_sleep_window(st, hour);
  if (s.asleep) {
    if (s.napping && s.energy >= 70) {
      s.napping = false;
      if (!night) {
        s.asleep = false;
        events |= EVT_WOKE_UP;
      }
    } else if (!s.napping && !night) {
      s.asleep = false;
      events |= EVT_WOKE_UP;
    }
  } else if (night && epoch >= s.woken_until) {
    s.asleep = true;
    s.napping = false;
    events |= EVT_FELL_ASLEEP;
  } else if (s.energy <= 5) {
    s.asleep = true;
    s.napping = true;
    events |= EVT_FELL_ASLEEP;
  }

  // Daytime: the room light comes back on and tonight's light mistake can happen again,
  // however the pet got through the night.
  if (!night && !s.asleep) {
    s.light_on = true;
    s.light_mistake_tonight = false;
  }

  // Decay.
  float awake_k = s.asleep ? 0.3f : 1.0f;
  s.hunger -= HUNGER_PER_H[st] * h * awake_k;
  float unhappy = HAPPY_PER_H[st];
  if (s.hunger < 20) unhappy += 4;
  if (s.hygiene < 30) unhappy += 3;
  if (s.sick) unhappy += 4;
  s.happiness -= unhappy * h * awake_k;
  if (s.asleep)
    s.energy += (s.light_on && !s.napping ? 7 : 14) * h;
  else
    s.energy -= 6 * h;
  s.hygiene -= (3 + 6 * s.poop) * h;

  // Poop.
  s.poop_timer_s += s.asleep ? dt / 2 : dt;
  if (s.poop_timer_s >= (uint32_t)(POOP_EVERY_H[st] * 3600)) {
    s.poop_timer_s = 0;
    if (s.poop < 4) {
      s.poop++;
      events |= EVT_POOPED;
    }
  }

  // Health.
  float damage = 0;
  if (s.hunger <= 0) damage += 8;
  if (s.poop >= 3) damage += 4;
  if (s.sick) damage += 5;
  if (s.hygiene < 15) damage += 3;
  if (damage > 0)
    s.health -= damage * h;
  else if (s.hunger > 30 && s.happiness > 30)
    s.health += 3 * h;
  clamp_stats(s);
  if (s.hunger < 50) s.overfeed = 0;  // only feeds in one over-full spell count towards sickness

  // Sickness.
  if (!s.sick) {
    bool got_sick = false;
    if (s.poop >= 2) got_sick |= timer_crossed(s.dirty_s, dt, 2 * 3600);
    else s.dirty_s = 0;
    if (s.hunger <= 0) got_sick |= timer_crossed(s.starving_s, dt, 3600);
    else s.starving_s = 0;
    if (got_sick) {
      s.sick = true;
      s.dirty_s = s.starving_s = 0;
      events |= EVT_GOT_SICK;
    }
  }

  // Care mistakes: a stat left at zero for 15 minutes, or the light left on at night.
  if (s.hunger <= 0 || s.happiness <= 0) {
    if (timer_crossed(s.zero_stat_s, dt, 15 * 60)) care_mistake(s, events);
  } else {
    s.zero_stat_s = 0;
  }
  if (s.asleep && !s.napping && s.light_on) {
    if (timer_crossed(s.light_sleep_s, dt, 30 * 60) && !s.light_mistake_tonight) {
      s.light_mistake_tonight = true;
      care_mistake(s, events);
    }
  } else {
    s.light_sleep_s = 0;
  }

  bool critical = s.hunger < 20 || s.happiness < 20 || s.sick || s.poop >= 2 || s.health < 30;
  if (critical && !s.critical) events |= EVT_NEEDS_ATTENTION;
  s.critical = critical;

  // Neglect: health at zero for 12 hours and the pet leaves; a new egg appears.
  if (s.health <= 0) {
    if (timer_crossed(s.neglect_s, dt, 12 * 3600)) {
      uint32_t coins = s.coins;
      uint32_t next_id = s.pet_id * 1664525u + 1013904223u;
      pet_init(s, next_id, epoch, s.generation + 1);
      s.coins = coins;
      return events | EVT_LEFT;
    }
  } else {
    s.neglect_s = 0;
  }

  if (STAGE_DURATION_S[st] && s.stage_s >= STAGE_DURATION_S[st]) events |= evolve(s);
  return events;
}

uint32_t pet_sim_advance(PetState &s, uint32_t now_epoch, HourFn hour_of) {
  if (now_epoch <= s.last_epoch) return 0;
  if (now_epoch - s.last_epoch > MAX_CATCHUP_S) s.last_epoch = now_epoch - MAX_CATCHUP_S;

  uint32_t events = 0;
  while (now_epoch - s.last_epoch >= STEP_S) {
    s.last_epoch += STEP_S;
    events |= step(s, STEP_S, s.last_epoch, hour_of(s.last_epoch));
  }
  return events;
}

Mood pet_mood(const PetState &s) {
  if (s.stage == STAGE_EGG) return MOOD_CONTENT;
  if (s.asleep) return MOOD_SLEEPING;
  if (s.sick) return MOOD_SICK;
  if (s.hunger < 25) return MOOD_HUNGRY;
  if (s.poop > 0 || s.hygiene < 30) return MOOD_DIRTY;
  if (s.happiness < 25) return MOOD_SAD;
  if (s.energy < 20) return MOOD_TIRED;
  if (s.happiness > 70 && s.hunger > 50) return MOOD_HAPPY;
  return MOOD_CONTENT;
}

const char *pet_form_name(uint8_t form) {
  static const char *const names[FORM_COUNT] = {"Egg",   "Blob", "Bun",  "Sprout", "Kitty",
                                                 "Mochi", "Ghost", "Star", "Drake",  "Jelly"};
  return form < FORM_COUNT ? names[form] : "?";
}

const char *pet_stage_name(uint8_t stage) {
  static const char *const names[] = {"Egg", "Baby", "Child", "Teen", "Adult"};
  return stage <= STAGE_ADULT ? names[stage] : "?";
}

bool pet_feed(PetState &s, Food food) {
  if (s.stage == STAGE_EGG || s.asleep || food >= FOOD_COUNT) return false;
  const FoodInfo &f = FOODS[food];
  if (s.coins < f.price) return false;
  s.coins -= f.price;
  if (s.hunger >= 95) {
    s.overfeed++;
    if (s.overfeed >= 3) {
      s.sick = true;
      s.overfeed = 0;
    }
  }
  s.hunger += f.hunger;
  s.happiness += f.happiness;
  s.health += f.health;
  s.weight += f.weight;
  clamp_stats(s);
  return true;
}

bool pet_clean(PetState &s) {
  if (s.stage == STAGE_EGG || (s.poop == 0 && s.hygiene >= 90)) return false;
  s.poop = 0;
  s.hygiene = 100;
  s.dirty_s = 0;
  s.happiness += 3;
  clamp_stats(s);
  return true;
}

bool pet_medicine(PetState &s) {
  if (s.stage == STAGE_EGG) return false;
  if (s.sick) {
    s.sick = false;
    s.health += 20;
    s.happiness -= 5;
  } else {
    s.happiness -= 10;  // bitter, and it wasn't needed
  }
  s.overfeed = 0;
  clamp_stats(s);
  return true;
}

void pet_toggle_light(PetState &s) { s.light_on = !s.light_on; }

void pet_pat(PetState &s) {
  if (s.stage == STAGE_EGG || s.asleep) return;
  s.happiness += 1.5f;
  clamp_stats(s);
}

void pet_wake(PetState &s, uint32_t now_epoch) {
  if (!s.asleep) return;
  s.asleep = false;
  s.napping = false;
  s.woken_until = now_epoch + 10 * 60;
  s.happiness -= 8;
  clamp_stats(s);
}

void pet_game_result(PetState &s, uint32_t score) {
  float joy = 5 + score / 2.0f;
  s.happiness += joy > 25 ? 25 : joy;
  s.energy -= 12;
  s.weight -= 1;
  if (s.weight < BASE_WEIGHT[s.stage]) s.weight = BASE_WEIGHT[s.stage];
  s.coins += score / 2 + 1;
  clamp_stats(s);
}

void pet_friend_visit(PetState &s, uint32_t gift_coins) {
  s.happiness += 20;
  s.coins += gift_coins;
  clamp_stats(s);
}

uint32_t pet_crc32(const void *data, size_t len) {
  const uint8_t *p = (const uint8_t *)data;
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; i++) {
    crc ^= p[i];
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
  }
  return ~crc;
}

void pet_blob_pack(const PetState &s, PetBlob &out) {
  memset(&out, 0, sizeof(out));
  out.version = PET_STATE_VERSION;
  memcpy(&out.state, &s, sizeof(s));
  out.crc = pet_crc32(&out, offsetof(PetBlob, crc));
}

bool pet_blob_unpack(const PetBlob &in, PetState &out) {
  if (in.version != PET_STATE_VERSION) return false;
  if (pet_crc32(&in, offsetof(PetBlob, crc)) != in.crc) return false;
  memcpy(&out, &in.state, sizeof(out));
  return true;
}
