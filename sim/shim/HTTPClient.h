#pragma once

#include "Arduino.h"
#include "WiFiClientSecure.h"

#define HTTP_CODE_OK 200

// Always succeeds and serves a canned response, so WeatherPlugin's real
// parsing, icon selection and drawing code all run unmodified.
struct HTTPClient {
  bool begin(WiFiClient &, const String &) { return true; }
  bool begin(const String &) { return true; }
  void setTimeout(unsigned long) {}
  int GET() { return HTTP_CODE_OK; }
  String getString();
  void end() {}
};
