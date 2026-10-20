#include "pet_save.h"

#include <Arduino.h>
#include <Preferences.h>

#define NVS_NAMESPACE "tomo"

bool pet_load(PetState &out) {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) return false;
  PetBlob blob;
  size_t len = prefs.getBytes("pet", &blob, sizeof(blob));
  prefs.end();
  if (len != sizeof(blob)) return false;
  return pet_blob_unpack(blob, out);
}

void pet_save(const PetState &s) {
  PetBlob blob;
  pet_blob_pack(s, blob);
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return;
  prefs.putBytes("pet", &blob, sizeof(blob));
  prefs.end();
}

int friends_load(FriendRecord *out) {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) return 0;
  size_t len = prefs.getBytes("friends", out, sizeof(FriendRecord) * MAX_FRIENDS);
  prefs.end();
  return len % sizeof(FriendRecord) == 0 ? len / sizeof(FriendRecord) : 0;
}

void friends_save(const FriendRecord *list, int count) {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return;
  prefs.putBytes("friends", list, sizeof(FriendRecord) * count);
  prefs.end();
}
