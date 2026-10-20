#pragma once

#include <stdint.h>

#include "pet/pet_sim.h"

// A nearby pet heard over BLE.
struct FriendSeen {
  uint32_t pet_id;
  uint8_t form, palette, stage, mood;
  char name[7];
  int8_t rssi;
};

// Starts advertising this pet's beacon and the periodic passive scan.
void friends_init(const PetState &pet);
// Refreshes the beacon if anything it carries has changed.
void friends_update_beacon(const PetState &pet);
// Starts a short scan every minute; call from loop().
void friends_tick();
// Pops one sighting reported by the scanner; false when none are pending.
bool friends_poll(FriendSeen &out);
