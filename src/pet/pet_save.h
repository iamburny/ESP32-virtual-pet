#pragma once

#include "pet_sim.h"

#define MAX_FRIENDS 16

struct FriendRecord {
  uint32_t pet_id;
  char name[7];
  uint8_t form, palette;
  uint32_t last_visit_epoch;
  uint16_t visits;
};

// Returns false if there is no valid saved pet (first boot, or a format change).
bool pet_load(PetState &out);
void pet_save(const PetState &s);

// Loads up to MAX_FRIENDS records; returns how many.
int friends_load(FriendRecord *out);
void friends_save(const FriendRecord *list, int count);
