#include "weather_store.h"
#include "config.h"

#include <algorithm>
#include <vector>

#ifdef SIMULATOR
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#endif
#ifdef ESP32
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#endif
#ifdef ESP8266
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#endif

#include <ArduinoJson.h>

namespace
{
constexpr unsigned long UPDATE_INTERVAL_MS = 1000UL * 60 * 30;

// https://github.com/chubin/wttr.in/blob/master/share/translations/en.txt
const std::vector<int> thunderCodes = {200, 386, 389, 392, 395};
const std::vector<int> cloudyCodes = {119, 122};
const std::vector<int> partlyCloudyCodes = {116};
const std::vector<int> clearCodes = {113};
const std::vector<int> fogCodes = {143, 248, 260};
const std::vector<int> rainCodes = {176, 293, 296, 299, 302, 305, 308, 311, 314,
                                    353, 356, 359, 386, 389, 263, 266, 281, 284, 185};
const std::vector<int> snowCodes = {179, 227, 323, 326, 329, 332,
                                    335, 338, 368, 371, 392, 395, 230, 350};

bool contains(const std::vector<int> &codes, int code)
{
  return std::find(codes.begin(), codes.end(), code) != codes.end();
}

/** Maps a wttr.in weather code onto an index into weatherIcons. */
int iconForCode(int code)
{
  if (contains(thunderCodes, code))
    return 1;
  if (contains(rainCodes, code))
    return 4;
  if (contains(snowCodes, code))
    return 5;
  if (contains(fogCodes, code))
    return 6;
  if (contains(clearCodes, code))
    return 2;
  if (contains(cloudyCodes, code))
    return 0;
  if (contains(partlyCloudyCodes, code))
    return 3;
  return 0;
}

#ifdef ESP32
WiFiClientSecure *secureClient = nullptr;
#endif
#ifdef ESP8266
WiFiClient wiFiClient;
#endif
} // namespace

WeatherStore &WeatherStore::getInstance()
{
  static WeatherStore instance;
  return instance;
}

WeatherStore &weatherStore = WeatherStore::getInstance();

bool WeatherStore::fetch()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[weather] WiFi not connected, skipping update");
    return false;
  }

  const String location = config.getWeatherLocation();
  const String url = "https://wttr.in/" + location + "?format=j2&lang=en";
  Serial.print("[weather] fetching ");
  Serial.println(url);

  HTTPClient http;

#ifdef ESP32
  if (secureClient == nullptr)
  {
    secureClient = new WiFiClientSecure();
    secureClient->setInsecure();
  }
  http.begin(*secureClient, url);
#endif
#ifdef ESP8266
  http.begin(wiFiClient, url);
#endif
#ifdef SIMULATOR
  http.begin(url);
#endif

  http.setTimeout(20000);

  const int code = http.GET();
  if (code != HTTP_CODE_OK)
  {
    Serial.print("[weather] HTTP request failed: ");
    Serial.println(code);
    http.end();
    return false;
  }

  const String payload = http.getString();

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, payload);
  if (error)
  {
    Serial.print("[weather] JSON parsing failed: ");
    Serial.println(error.c_str());
    http.end();
    return false;
  }

  reading_.temperatureC = round(doc["current_condition"][0]["temp_C"].as<float>());
  reading_.icon = iconForCode(doc["current_condition"][0]["weatherCode"].as<int>());
  reading_.valid = true;

  http.end();
  return true;
}

void WeatherStore::update()
{
  if (everFetched_ && millis() < lastUpdate_ + UPDATE_INTERVAL_MS)
  {
    return;
  }

  everFetched_ = true;
  lastUpdate_ = millis();
  fetch();
}
