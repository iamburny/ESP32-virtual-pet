#include "app.h"

#include <Arduino.h>
#include <time.h>

#include "net/net_task.h"
#include "ui/ui.h"

#define VALID_EPOCH 1700000000UL
#define SAVE_DELAY_MS 3000
#define SAVE_EVERY_MS (5UL * 60 * 1000)
#define VISIT_COOLDOWN_S (6UL * 3600)
#define VISIT_MIN_RSSI -75

PetState g_pet;
FriendRecord g_friends[MAX_FRIENDS];
int g_friend_count;

static uint32_t base_epoch;
static int64_t base_us;
static bool synced;
static uint32_t last_sim_ms, save_due_ms, last_save_ms;
static bool dirty;
static uint32_t shake_times[3];

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

static void handle_friend(const FriendSeen &f) {
  if (f.rssi < VISIT_MIN_RSSI || f.pet_id == g_pet.pet_id) return;

  FriendRecord *rec = nullptr;
  for (int i = 0; i < g_friend_count; i++)
    if (g_friends[i].pet_id == f.pet_id) rec = &g_friends[i];

  uint32_t now = app_epoch();
  bool due = !rec || now - rec->last_visit_epoch >= VISIT_COOLDOWN_S;
  if (!due || g_pet.stage == STAGE_EGG || g_pet.asleep || !ui_idle_on_home()) return;

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

  uint32_t gift = 10 + (g_pet.pet_id ^ f.pet_id ^ (now / 86400)) % 21;
  bool first = rec->visits == 0;
  rec->last_visit_epoch = now;
  rec->visits++;
  pet_friend_visit(g_pet, gift);
  friends_save(g_friends, g_friend_count);
  app_mark_dirty();
  Serial.printf("[app] friend visit from %s (rssi %d), gift %lu\n", f.name, f.rssi, (unsigned long)gift);
  ui_friend_visit(f, gift, first);
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
      ui_on_events(events);
      app_mark_dirty();
      if (g_pet.stage != stage) save_now();
    }
    friends_update_beacon(g_pet);
  }

  if ((dirty && (int32_t)(ms - save_due_ms) >= 0) || ms - last_save_ms >= SAVE_EVERY_MS) save_now();

  FriendSeen f;
  while (friends_poll(f)) handle_friend(f);
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
