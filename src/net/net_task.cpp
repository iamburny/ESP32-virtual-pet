#include "net_task.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <time.h>

#include "secrets.h"
#include "wifi_link.h"

#define SYNC_INTERVAL_MS (30UL * 60 * 1000)
#define RETRY_MIN_MS (60UL * 1000)
#define RETRY_MAX_MS (15UL * 60 * 1000)
#define VALID_EPOCH 1700000000
#define BUSY_POLL_MS 5000
#define BUSY_MAX_WAIT_MS (15UL * 60 * 1000)

static SemaphoreHandle_t mutex;
static NetState state;
static volatile bool busy;

static uint8_t wmo_to_weather(int code) {
  if (code == 0) return WX_CLEAR;
  if (code <= 2) return WX_PARTLY;
  if (code == 3) return WX_CLOUDY;
  if (code == 45 || code == 48) return WX_FOG;
  if (code >= 51 && code <= 57) return WX_DRIZZLE;
  if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return WX_RAIN;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return WX_SNOW;
  if (code >= 95) return WX_STORM;
  return WX_UNKNOWN;
}

static bool fetch_weather() {
  HTTPClient http;
  http.setTimeout(8000);
  http.useHTTP10(true);  // no chunked encoding, so the body can be parsed straight from the stream
  http.begin(String("http://api.open-meteo.com/v1/forecast?latitude=") + WEATHER_LAT + "&longitude=" + WEATHER_LON +
             "&current=temperature_2m,weather_code,is_day");
  int code = http.GET();
  if (code != 200) {
    Serial.printf("[net] weather HTTP %d\n", code);
    http.end();
    return false;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err || !doc["current"].is<JsonObject>()) {
    Serial.println("[net] weather parse failed");
    return false;
  }

  JsonObject cur = doc["current"];
  xSemaphoreTake(mutex, portMAX_DELAY);
  state.weather_valid = true;
  state.temp_c = cur["temperature_2m"] | 0.0f;
  state.weather = wmo_to_weather(cur["weather_code"] | -1);
  state.is_day = (cur["is_day"] | 1) != 0;
  state.weather_epoch = time(nullptr);
  xSemaphoreGive(mutex);
  Serial.printf("[net] weather %s %.1fC\n", weather_name(state.weather), state.temp_c);
  return true;
}

static bool sync_time() {
  configTzTime(TZ_POSIX, "pool.ntp.org", "time.google.com");
  for (int i = 0; i < 150 && time(nullptr) < VALID_EPOCH; i++) delay(100);
  return net_time_valid();
}

static void net_task(void *) {
  uint32_t retry_ms = RETRY_MIN_MS;
  for (;;) {
    // Bringing Wi-Fi up starves the UI for a second or two, so once the clock
    // is set, wait for the user to leave the device alone first.
    for (uint32_t waited = 0; net_time_valid() && busy && waited < BUSY_MAX_WAIT_MS; waited += BUSY_POLL_MS)
      vTaskDelay(pdMS_TO_TICKS(BUSY_POLL_MS));

    bool ok = false;
    if (wifi_up(20000)) {
      bool time_ok = sync_time();
      bool weather_ok = fetch_weather();
      ok = time_ok && weather_ok;
    }
    wifi_down();

    uint32_t wait_ms = ok ? SYNC_INTERVAL_MS : retry_ms;
    retry_ms = ok ? RETRY_MIN_MS : min<uint32_t>(retry_ms * 2, RETRY_MAX_MS);
    vTaskDelay(pdMS_TO_TICKS(wait_ms));
  }
}

void net_start() {
  setenv("TZ", TZ_POSIX, 1);
  tzset();
  mutex = xSemaphoreCreateMutex();
  xTaskCreate(net_task, "net", 8192, nullptr, 1, nullptr);
}

NetState net_get() {
  if (!mutex) return state;
  xSemaphoreTake(mutex, portMAX_DELAY);
  NetState copy = state;
  xSemaphoreGive(mutex);
  return copy;
}

void net_set_busy(bool b) { busy = b; }

bool net_time_valid() { return time(nullptr) >= VALID_EPOCH; }

const char *weather_name(uint8_t wx) {
  static const char *const names[] = {"?", "Clear", "Partly cloudy", "Cloudy", "Fog", "Drizzle", "Rain", "Snow", "Storm"};
  return wx <= WX_STORM ? names[wx] : "?";
}
