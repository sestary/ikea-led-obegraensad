#pragma once

#include "Arduino.h"
#include <type_traits>
#include "sim_weather.h"

// Minimal stand-in for ArduinoJson. WeatherPlugin's entire JSON surface is
//   doc["current_condition"][0]["temp_C"].as<float>()
//   doc["current_condition"][0]["weatherCode"].as<int>()
// so the proxy remembers the last string key and resolves those two from the
// canned condition, ignoring the payload entirely.
struct JsonValue {
  const char *key = "";

  JsonValue operator[](const char *k) const { return JsonValue{k}; }
  JsonValue operator[](int) const { return JsonValue{key}; }

  template <class T> T as() const {
    if constexpr (std::is_arithmetic<T>::value) {
      if (std::strcmp(key, "temp_C") == 0)
        return static_cast<T>(simWeatherTempC());
      if (std::strcmp(key, "weatherCode") == 0)
        return static_cast<T>(simWeatherCode());
      if (std::strcmp(key, "moon_illumination") == 0)
        return static_cast<T>(simMoonIllumination());
      return static_cast<T>(0);
    } else {
      if (std::strcmp(key, "moon_phase") == 0)
        return T{simMoonPhaseName()};
      return T{};
    }
  }

  // The rest of this type exists only so AnimationPlugin::websocketHook
  // compiles. The simulator has no websocket, so it is never called.
  template <class T> bool is() const { return false; }
  size_t size() const { return 0; }
  void clear() {}

  // One arithmetic conversion only: separate int and float operators would
  // make a conversion to uint8_t ambiguous.
  template <class T, class = std::enable_if_t<std::is_arithmetic<T>::value>>
  operator T() const {
    return as<T>();
  }
  operator const char *() const { return ""; }

  template <class T> JsonValue &operator=(const T &) { return *this; }

  JsonValue add() { return *this; }
  JsonValue createNestedArray() { return *this; }
  JsonValue createNestedObject() { return *this; }

  JsonValue *begin() { return this; }
  JsonValue *end() { return this; }
};

struct DeserializationError {
  explicit operator bool() const { return false; }
  const char *c_str() const { return "ok"; }
};

// Iterating a JsonArray yields nothing: the simulator never receives scheduler
// or animation payloads, so these exist purely to satisfy the compiler.
using JsonArray = JsonValue;
using JsonObject = JsonValue;

struct JsonDocument {
  JsonValue operator[](const char *k) const { return JsonValue{k}; }
  void clear() {}
  size_t size() const { return 0; }

  template <class T> T as() const { return T{}; }
  template <class T> bool is() const { return false; }
};

inline DeserializationError deserializeJson(JsonDocument &, const String &) {
  return DeserializationError{};
}

inline size_t serializeJson(const JsonDocument &, String &out) {
  out = "{}";
  return out.size();
}
