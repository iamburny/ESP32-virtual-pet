#include "app.h"

#include <Arduino.h>
#include <time.h>

#include "net/net_task.h"
#include "ui/ui.h"

#define VALID_EPOCH 1700000000UL
#define SAVE_DELAY_MS 3000
#define SAVE_EVERY_MS (5UL * 60 * 1000)
#define GIFT_COOLDOWN_S (6UL * 3600)
#define NEARBY_MIN_RSSI -80
#define MAX_NEARBY 6
// Scans run every minute (every 20 s while a guest is over); allow a couple of misses.
#define NEARBY_LOST_MS 150000
#define GUEST_LOST_MS 65000

PetState g_pet;
FriendRecord g_friends[MAX_FRIENDS];
int g_friend_count;

static uint32_t base_epoch;
static int64_t base_us;
static bool synced;
static uint32_t last_sim_ms, save_due_ms, last_save_ms;
static bool dirty;
static uint32_t shake_times[3];

struct NearbyPal {
  FriendSeen seen;
  uint32_t last_seen_ms;
};
static NearbyPal nearby[MAX_NEARBY];
static int nearby_count;
static uint32_t guest_id;

uint32_t app_epoch() { return base_epoch + (uint32_t)((esp_timer_get_time() - base_us) / 1000000); }
bool app_clock_synced() { return synced; }

int app_local_hour(uint32_t epoch) {
  // Until NTP has synced this boot the clock may be stale; treat it as midday.
  if (!synced || epoch < VALID_EPOCH) return 12;
  time_t t = epoch;
  struct tm tm;
  localtime_r(&t, &tm);
  return tm.tm_hour;
}

void app_mark_dirty() {
  if (!dirty) save_due_ms = millis() + SAVE_DELAY_MS;
  dirty = true;
}

static void save_now() {
  pet_save(g_pet);
  dirty = false;
  last_save_ms = millis();
}

void app_init() {
  if (pet_load(g_pet)) {
    Serial.printf("[app] loaded %s (%s %s), last saved at %lu\n", g_pet.name, pet_stage_name(g_pet.stage),
                  pet_form_name(g_pet.form), (unsigned long)g_pet.last_epoch);
  } else {
    // A fresh egg runs on a relative clock until NTP gives real time.
    pet_init(g_pet, esp_random(), 1000);
    Serial.println("[app] no saved pet, new egg");
    save_now();
  }
  g_friend_count = friends_load(g_friends);
  base_epoch = g_pet.last_epoch;
  base_us = esp_timer_get_time();
  last_save_ms = millis();
}

void app_start_over() {
  app_send_guest_home();
  if (synced) {
    pet_init(g_pet, esp_random(), app_epoch());
  } else {
    // Until NTP syncs, app_epoch() may be a stale saved time; put the egg on the
    // relative clock so sync_clock() shifts it rather than catching it up.
    pet_init(g_pet, esp_random(), 1000);
    base_epoch = 1000;
    base_us = esp_timer_get_time();
  }
  save_now();
  Serial.println("[app] started over with a new egg");
}

static void sync_clock() {
  uint32_t now = time(nullptr);
  int64_t delta = (int64_t)now - app_epoch();
  if (g_pet.last_epoch < VALID_EPOCH) {
    // The pet has only ever known relative time: shift it onto real time
    // without treating the gap as time that passed.
    g_pet.last_epoch += delta;
    if (g_pet.woken_until) g_pet.woken_until += delta;
    for (int i = 0; i < g_friend_count; i++)
      if (g_friends[i].last_visit_epoch && g_friends[i].last_visit_epoch < VALID_EPOCH)
        g_friends[i].last_visit_epoch += delta;
  }
  Serial.printf("[app] clock synced; %ld s to catch up\n", (long)(now - g_pet.last_epoch));
  base_epoch = now;
  base_us = esp_timer_get_time();
  synced = true;
  app_mark_dirty();
}

static NearbyPal *find_nearby(uint32_t pet_id) {
  for (int i = 0; i < nearby_count; i++)
    if (nearby[i].seen.pet_id == pet_id) return &nearby[i];
  return nullptr;
}

static void handle_sighting(const FriendSeen &f) {
  if (f.rssi < NEARBY_MIN_RSSI || f.pet_id == g_pet.pet_id) return;
  NearbyPal *n = find_nearby(f.pet_id);
  if (!n) {
    if (nearby_count == MAX_NEARBY) return;
    n = &nearby[nearby_count++];
    Serial.printf("[app] %s is nearby (rssi %d)\n", f.name, f.rssi);
    if (g_pet.stage != STAGE_EGG) ui_pal_nearby(f);
  }
  n->seen = f;
  n->last_seen_ms = millis();
  if (f.pet_id == guest_id) ui_guest_update(f);
}

// Drops pals that haven't been heard for a while; a guest that drops out goes home.
static void expire_nearby() {
  uint32_t ms = millis();
  for (int i = 0; i < nearby_count;) {
    bool guest = nearby[i].seen.pet_id == guest_id;
    if (ms - nearby[i].last_seen_ms < (guest ? GUEST_LOST_MS : NEARBY_LOST_MS)) {
      i++;
      continue;
    }
    Serial.printf("[app] %s is no longer nearby\n", nearby[i].seen.name);
    if (guest) app_send_guest_home();
    nearby[i] = nearby[--nearby_count];
  }
}

int app_nearby_count() { return nearby_count; }
const FriendSeen &app_nearby(int i) { return nearby[i].seen; }
uint32_t app_guest_id() { return guest_id; }

bool app_invite(uint32_t pet_id) {
  NearbyPal *n = find_nearby(pet_id);
  if (!n || g_pet.stage == STAGE_EGG || g_pet.asleep) return false;
  if (guest_id) app_send_guest_home();
  n->last_seen_ms = millis();  // give the fast scans a full window to hear the guest
  const FriendSeen &f = n->seen;

  FriendRecord *rec = nullptr;
  for (int i = 0; i < g_friend_count; i++)
    if (g_friends[i].pet_id == f.pet_id) rec = &g_friends[i];
  if (!rec) {
    if (g_friend_count < MAX_FRIENDS) {
      rec = &g_friends[g_friend_count++];
    } else {
      rec = &g_friends[0];
      for (int i = 1; i < g_friend_count; i++)
        if (g_friends[i].last_visit_epoch < rec->last_visit_epoch) rec = &g_friends[i];
    }
    memset(rec, 0, sizeof(*rec));
    rec->pet_id = f.pet_id;
  }
  memcpy(rec->name, f.name, sizeof(rec->name));
  rec->form = f.form;
  rec->palette = f.palette;

  // A gift (and happiness boost) at most once per cooldown per pal, however often
  // they're invited. Signed, so a pre-sync relative clock behind a real-time
  // record never counts as due.
  uint32_t now = app_epoch();
  bool first = rec->visits == 0;
  uint32_t gift = 0;
  if (first || (int32_t)(now - rec->last_visit_epoch) >= (int32_t)GIFT_COOLDOWN_S) {
    gift = 10 + (g_pet.pet_id ^ f.pet_id ^ (now / 86400)) % 21;
    rec->last_visit_epoch = now;
  }
  rec->visits++;
  if (gift) pet_friend_visit(g_pet, gift);
  friends_save(g_friends, g_friend_count);
  app_mark_dirty();

  guest_id = f.pet_id;
  friends_fast_scan(true);
  Serial.printf("[app] %s came over, gift %lu\n", f.name, (unsigned long)gift);
  ui_guest_arrive(f, gift, first);
  return true;
}

void app_send_guest_home() {
  if (!guest_id) return;
  guest_id = 0;
  friends_fast_scan(false);
  ui_guest_leave();
}

void app_loop() {
  uint32_t ms = millis();

  if (!synced && net_time_valid()) sync_clock();

  if (ms - last_sim_ms >= 1000) {
    last_sim_ms = ms;
    uint8_t stage = g_pet.stage;
    uint32_t events = pet_sim_advance(g_pet, app_epoch(), app_local_hour);
    if (events) {
      Serial.printf("[app] events 0x%03lx stage %s form %s\n", (unsigned long)events, pet_stage_name(g_pet.stage),
                    pet_form_name(g_pet.form));
      if (events & EVT_LEFT) app_send_guest_home();
      ui_on_events(events);
      app_mark_dirty();
      if (g_pet.stage != stage) save_now();
    }
    friends_update_beacon(g_pet);
  }

  if ((dirty && (int32_t)(ms - save_due_ms) >= 0) || ms - last_save_ms >= SAVE_EVERY_MS) save_now();

  FriendSeen f;
  while (friends_poll(f)) handle_sighting(f);
  expire_nearby();
}

bool app_feed(Food food) {
  bool ok = pet_feed(g_pet, food);
  if (ok) app_mark_dirty();
  return ok;
}

bool app_clean() {
  bool ok = pet_clean(g_pet);
  if (ok) app_mark_dirty();
  return ok;
}

bool app_medicine() {
  bool ok = pet_medicine(g_pet);
  if (ok) app_mark_dirty();
  return ok;
}

void app_toggle_light() {
  pet_toggle_light(g_pet);
  app_mark_dirty();
}

void app_pat() {
  pet_pat(g_pet);
  app_mark_dirty();
}

void app_shaken() {
  uint32_t ms = millis();
  if (g_pet.asleep) {
    pet_wake(g_pet, app_epoch());
  } else {
    // Three shakes within 30 s upsets the pet.
    shake_times[0] = shake_times[1];
    shake_times[1] = shake_times[2];
    shake_times[2] = ms;
    if (shake_times[0] && ms - shake_times[0] < 30000) {
      g_pet.happiness = g_pet.happiness > 5 ? g_pet.happiness - 5 : 0;
      shake_times[0] = shake_times[1] = shake_times[2] = 0;
    }
  }
  app_mark_dirty();
}

void app_game_over(uint32_t score) {
  pet_game_result(g_pet, score);
  app_mark_dirty();
}
