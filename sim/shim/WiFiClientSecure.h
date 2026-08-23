#pragma once

#include "WiFi.h"

struct WiFiClientSecure : WiFiClient {
  void setInsecure() {}
  void setTimeout(unsigned long) {}
};
