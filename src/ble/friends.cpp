#include "friends.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <string.h>

// Manufacturer-specific data layout (19 bytes):
//   [0..1] company id 0xFFFF (reserved for testing), [2..3] "TM", [4] version,
//   [5..8] pet id (LE), [9] form, [10] palette, [11] stage, [12] mood, [13..18] name.
#define BEACON_LEN 19
#define BEACON_VERSION 1
#define SCAN_EVERY_MS 60000
#define SCAN_EVERY_FAST_MS 20000
#define SCAN_FOR_MS 5000

static QueueHandle_t seen_queue;
static uint8_t beacon[BEACON_LEN];
static uint32_t last_scan_ms;
static bool started, fast;

static void build_beacon(const PetState &pet, uint8_t *out) {
  memset(out, 0, BEACON_LEN);
  out[0] = 0xFF;
  out[1] = 0xFF;
  out[2] = 'T';
  out[3] = 'M';
  out[4] = BEACON_VERSION;
  memcpy(out + 5, &pet.pet_id, 4);
  out[9] = pet.form;
  out[10] = pet.palette;
  out[11] = pet.stage;
  out[12] = pet_mood(pet);
  strncpy((char *)out + 13, pet.name, 6);
}

class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice *dev) override {
    if (!dev->haveManufacturerData()) return;
    std::string d = dev->getManufacturerData();
    if (d.size() != BEACON_LEN) return;
    const uint8_t *p = (const uint8_t *)d.data();
    if (p[0] != 0xFF || p[1] != 0xFF || p[2] != 'T' || p[3] != 'M' || p[4] != BEACON_VERSION) return;

    FriendSeen f = {};
    memcpy(&f.pet_id, p + 5, 4);
    f.form = p[9] < FORM_COUNT ? p[9] : FORM_BLOB;
    f.palette = p[10];
    f.stage = p[11];
    f.mood = p[12];
    memcpy(f.name, p + 13, 6);
    f.name[6] = 0;
    f.rssi = dev->getRSSI();
    xQueueSend(seen_queue, &f, 0);
  }
};
static ScanCallbacks scan_callbacks;

static void restart_advertising() {
  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->stop();
  NimBLEAdvertisementData data;
  data.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  data.setManufacturerData(beacon, BEACON_LEN);
  adv->setAdvertisementData(data);
  adv->setConnectableMode(BLE_GAP_CONN_MODE_NON);
  adv->start();
}

void friends_init(const PetState &pet) {
  seen_queue = xQueueCreate(8, sizeof(FriendSeen));
  NimBLEDevice::init("Tomo");

  NimBLEScan *scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&scan_callbacks, false);
  scan->setActiveScan(false);
  scan->setInterval(160);
  scan->setWindow(60);
  scan->setMaxResults(0);

  build_beacon(pet, beacon);
  restart_advertising();
  started = true;
  last_scan_ms = millis() - SCAN_EVERY_MS + 5000;  // first scan shortly after boot
  Serial.println("[ble] advertising pet beacon");
}

void friends_update_beacon(const PetState &pet) {
  if (!started) return;
  uint8_t next[BEACON_LEN];
  build_beacon(pet, next);
  if (memcmp(next, beacon, BEACON_LEN) == 0) return;
  memcpy(beacon, next, BEACON_LEN);
  restart_advertising();
}

void friends_tick() {
  if (!started) return;
  uint32_t now = millis();
  if (now - last_scan_ms < (fast ? SCAN_EVERY_FAST_MS : SCAN_EVERY_MS)) return;
  last_scan_ms = now;
  NimBLEScan *scan = NimBLEDevice::getScan();
  if (!scan->isScanning()) scan->start(SCAN_FOR_MS, false, true);
}

void friends_fast_scan(bool on) { fast = on; }

bool friends_poll(FriendSeen &out) { return started && xQueueReceive(seen_queue, &out, 0) == pdTRUE; }
