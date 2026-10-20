#pragma once

// Pure game logic for the pet. No Arduino or LVGL dependencies so it can be
// unit-tested on the host (`pio test -e native`).

#include <stddef.h>
#include <stdint.h>

enum Stage : uint8_t { STAGE_EGG, STAGE_BABY, STAGE_CHILD, STAGE_TEEN, STAGE_ADULT };

// Body shape / accessories the renderer draws. Chosen at each evolution from care quality.
enum Form : uint8_t {
  FORM_EGG,
  FORM_BLOB,    // baby
  FORM_BUN,     // child, good care
  FORM_SPROUT,  // child, poor care
  FORM_KITTY,   // teen
  FORM_MOCHI,   // teen
  FORM_GHOST,   // teen/adult, poor care
  FORM_STAR,    // adult
  FORM_DRAKE,   // adult, best care
  FORM_JELLY,   // adult, average care
  FORM_COUNT
};

enum Mood : uint8_t { MOOD_HAPPY, MOOD_CONTENT, MOOD_HUNGRY, MOOD_SAD, MOOD_DIRTY, MOOD_SICK, MOOD_SLEEPING, MOOD_TIRED };

enum Food : uint8_t { FOOD_MEAL, FOOD_FRUIT, FOOD_SNACK, FOOD_CAKE, FOOD_COUNT };

struct FoodInfo {
  const char *name;
  uint8_t price;
  int8_t hunger, happiness, health, weight;
};
extern const FoodInfo FOODS[FOOD_COUNT];

// Event flags returned by pet_sim_advance() so the UI can react.
enum : uint32_t {
  EVT_HATCHED = 1 << 0,
  EVT_EVOLVED = 1 << 1,
  EVT_POOPED = 1 << 2,
  EVT_GOT_SICK = 1 << 3,
  EVT_FELL_ASLEEP = 1 << 4,
  EVT_WOKE_UP = 1 << 5,
  EVT_LEFT = 1 << 6,          // neglected too long: pet left, a new egg replaced it
  EVT_CARE_MISTAKE = 1 << 7,
  EVT_NEEDS_ATTENTION = 1 << 8,  // a stat just dropped into the critical zone
};

#define PET_STATE_VERSION 1
#define PET_NAME_LEN 8

struct PetState {
  uint32_t pet_id;
  char name[PET_NAME_LEN];
  uint8_t stage, form, palette, generation;

  // 0..100, higher is better (hunger 100 = full).
  float hunger, happiness, energy, hygiene, health;
  float weight;

  uint32_t last_epoch;   // wall-clock second the state was last advanced to
  uint32_t age_s;        // seconds since hatching
  uint32_t stage_s;      // seconds in the current stage
  uint32_t coins;

  uint8_t poop;
  bool sick, asleep, light_on;
  uint16_t care_mistakes;   // lifetime
  uint16_t stage_mistakes;  // since last evolution; drives the next form
  uint8_t overfeed;         // feeds while already full; enough of them makes it sick

  // Internal timers (seconds) so the rules stay deterministic.
  uint32_t poop_timer_s;
  uint32_t dirty_s;         // continuous time with 2+ poops
  uint32_t starving_s;      // continuous time with hunger at 0
  uint32_t zero_stat_s;     // continuous time with hunger or happiness at 0
  uint32_t light_sleep_s;   // continuous time asleep with the light on
  uint32_t neglect_s;       // continuous time with health at 0
  uint32_t woken_until;     // epoch; stays awake at night until then
  bool light_mistake_tonight;
  bool critical;            // a stat is in the critical zone (edge-detects EVT_NEEDS_ATTENTION)
  bool napping;             // asleep because energy ran out rather than night time
};

// Returns the local hour (0-23) for a wall-clock epoch.
typedef int (*HourFn)(uint32_t epoch);

void pet_init(PetState &s, uint32_t pet_id, uint32_t now_epoch, uint8_t generation = 0);

// Advances the simulation to `now_epoch` in 60 s steps (capped at 7 days) and
// returns the OR of all events that fired.
uint32_t pet_sim_advance(PetState &s, uint32_t now_epoch, HourFn hour_of);

Mood pet_mood(const PetState &s);
const char *pet_form_name(uint8_t form);
const char *pet_stage_name(uint8_t stage);

// Player actions. Each returns false if the action is not possible right now.
bool pet_feed(PetState &s, Food food);
bool pet_clean(PetState &s);
bool pet_medicine(PetState &s);
void pet_toggle_light(PetState &s);
void pet_pat(PetState &s);   // single pat/stroke tick; caller rate-limits
void pet_wake(PetState &s, uint32_t now_epoch);  // jolted awake (shake)
void pet_game_result(PetState &s, uint32_t score);
void pet_friend_visit(PetState &s, uint32_t gift_coins);

// Versioned, CRC-protected blob for persistence.
struct PetBlob {
  uint32_t version;
  PetState state;
  uint32_t crc;
};
void pet_blob_pack(const PetState &s, PetBlob &out);
bool pet_blob_unpack(const PetBlob &in, PetState &out);
uint32_t pet_crc32(const void *data, size_t len);
