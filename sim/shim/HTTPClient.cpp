#include "HTTPClient.h"

#include "WiFi.h"
#include "sim_weather.h"

WiFiClass WiFi;

namespace {
struct Condition {
  int tempC;
  int code;
  const char *label;
};

// wttr.in weather codes, one per icon branch in WeatherPlugin.
// Non-const: simSetWeather() pins slot 0 so tests can assert a known reading.
Condition kConditions[] = {
    {21, 113, "clear"}, {18, 116, "partly cloudy"}, {14, 122, "overcast"},
    {9, 296, "rain"},   {3, 338, "snow"},           {16, 200, "thunder"},
    {11, 248, "fog"},
};

int g_index = 0;
} // namespace

void simSetWeather(int tempC, int weatherCode) {
  kConditions[0].tempC = tempC;
  kConditions[0].code = weatherCode;
  g_index = 0;
}

int simWeatherTempC() { return kConditions[g_index].tempC; }
int simWeatherCode() { return kConditions[g_index].code; }

int simWeatherCount() {
  return static_cast<int>(sizeof(kConditions) / sizeof(kConditions[0]));
}

const char *simWeatherLabel() { return kConditions[g_index].label; }

void simCycleWeather() { g_index = (g_index + 1) % simWeatherCount(); }

String HTTPClient::getString() {
  // The payload's shape does not matter: the stub JsonDocument answers from the
  // canned condition directly. Returned only so length logging works.
  return "{\"current_condition\":[{}]}";
}
