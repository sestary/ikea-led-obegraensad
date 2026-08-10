#include "Arduino.h"

#include "sim_clock.h"

SerialShim Serial;

namespace {
bool g_verbose = false;
}

void simSerialSetVerbose(bool verbose) { g_verbose = verbose; }
bool simSerialVerbose() { return g_verbose; }

unsigned long millis() { return simClockNow(); }
unsigned long micros() { return simClockNow() * 1000UL; }

// Effects are driven by the simulator's frame loop rather than by blocking
// waits. Advancing the simulated clock keeps NonBlockingDelay sensible without
// stalling the TUI.
void delay(unsigned long ms) { simClockStep(ms); }
void delayMicroseconds(unsigned long) {}

// No RTOS to yield to; the simulator is single-threaded.
void yield() {}

long random(long howbig) {
  if (howbig <= 0)
    return 0;
  return std::rand() % howbig;
}

long random(long howsmall, long howbig) {
  if (howbig <= howsmall)
    return howsmall;
  return howsmall + (std::rand() % (howbig - howsmall));
}

void randomSeed(unsigned long seed) { std::srand(static_cast<unsigned>(seed)); }

void pinMode(int, int) {}
void digitalWrite(int, int) {}
int digitalRead(int) { return 0; }

bool getLocalTime(struct tm *info, uint32_t) {
  std::time_t now = std::time(nullptr);
  localtime_r(&now, info);
  return true;
}

void configTime(long, int, const char *) {}
void configTzTime(const char *, const char *) {}
