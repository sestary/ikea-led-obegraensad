#pragma once

#include "Arduino.h"

#define WL_CONNECTED 3
#define WIFI_STA 1

// Always reports connected: the simulator serves canned data locally, and
// plugins that check status() should take their normal code path.
struct WiFiClass {
  int status() const { return WL_CONNECTED; }
  String localIP() const { return "127.0.0.1"; }
  String macAddress() const { return "00:00:00:00:00:00"; }
  void mode(int) {}
  void begin(const char * = nullptr, const char * = nullptr) {}
};

extern WiFiClass WiFi;

struct WiFiClient {};
