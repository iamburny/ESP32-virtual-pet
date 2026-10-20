#pragma once

#include <stdint.h>

enum Weather : uint8_t { WX_UNKNOWN, WX_CLEAR, WX_PARTLY, WX_CLOUDY, WX_FOG, WX_DRIZZLE, WX_RAIN, WX_SNOW, WX_STORM };

struct NetState {
  bool weather_valid;
  float temp_c;
  uint8_t weather;  // Weather
  bool is_day;
  uint32_t weather_epoch;
};

// Sets the timezone and starts the background task that periodically brings
// Wi-Fi up, syncs NTP, fetches the weather, then turns Wi-Fi off again.
void net_start();
NetState net_get();
// While busy, periodic syncs are deferred (up to 15 minutes); the first sync
// after boot is not.
void net_set_busy(bool busy);
// True once the system clock has been set from NTP.
bool net_time_valid();
const char *weather_name(uint8_t wx);
