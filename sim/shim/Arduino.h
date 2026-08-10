#pragma once

// Host stand-in for the Arduino core. Deliberately minimal: only the surface
// the firmware's screen, signs and plugin sources actually touch.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

#define IRAM_ATTR
#define DRAM_ATTR
#define PROGMEM

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif

#define OUTPUT 1
#define INPUT 0
#define INPUT_PULLUP 2
#define HIGH 1
#define LOW 0

typedef uint8_t byte;

using std::abs;
using std::max;
using std::min;

template <class T, class L, class H> T constrain(T v, L lo, H hi) {
  return v < static_cast<T>(lo)   ? static_cast<T>(lo)
         : v > static_cast<T>(hi) ? static_cast<T>(hi)
                                  : v;
}

// Arduino's integer re-range helper.
inline long map(long x, long inMin, long inMax, long outMin, long outMax) {
  if (inMax == inMin)
    return outMin;
  return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned long us);
void yield();

long random(long howbig);
long random(long howsmall, long howbig);
void randomSeed(unsigned long seed);

void pinMode(int pin, int mode);
void digitalWrite(int pin, int value);
int digitalRead(int pin);

// Host wall clock, so the clock plugins render the real local time.
bool getLocalTime(struct tm *info, uint32_t ms = 5000);
void configTime(long gmtOffset, int daylightOffset, const char *server);
void configTzTime(const char *tz, const char *server);

// Arduino's String is close enough to std::string for the firmware's usage.
using String = std::string;

// Serial writes would corrupt the TUI, so they are dropped unless the simulator
// is started with --verbose, which redirects them to stderr.
void simSerialSetVerbose(bool verbose);
bool simSerialVerbose();

struct SerialShim {
  void begin(unsigned long) {}
  void flush() {}
  explicit operator bool() const { return true; }

  void print(const std::string &s) { emit("%s", s.c_str()); }
  void print(const char *s) { emit("%s", s); }
  void print(char c) { emit("%c", c); }
  void print(int v) { emit("%d", v); }
  void print(unsigned v) { emit("%u", v); }
  void print(long v) { emit("%ld", v); }
  void print(unsigned long v) { emit("%lu", v); }
  void print(double v) { emit("%f", v); }

  template <class T> void println(T v) {
    print(v);
    emit("%s", "\n");
  }
  void println() { emit("%s", "\n"); }

  template <class... A> void printf(const char *fmt, A... args) {
    if (simSerialVerbose())
      std::fprintf(stderr, fmt, args...);
  }

private:
  template <class... A> void emit(const char *fmt, A... args) {
    if (simSerialVerbose())
      std::fprintf(stderr, fmt, args...);
  }
};

extern SerialShim Serial;
