#include "sim_clock.h"

#include <chrono>

namespace {
using Clock = std::chrono::steady_clock;

Clock::time_point g_lastReal = Clock::now();
double g_simMs = 0.0;
double g_speed = 1.0;
bool g_paused = false;

// Fold the real time elapsed since the last observation into the simulated
// clock, then rebase. Called before every read or state change so that changing
// speed or pausing never retroactively rescales time already elapsed.
void accumulate() {
  auto now = Clock::now();
  if (!g_paused) {
    double realMs =
        std::chrono::duration<double, std::milli>(now - g_lastReal).count();
    g_simMs += realMs * g_speed;
  }
  g_lastReal = now;
}
} // namespace

void simClockSetSpeed(double factor) {
  accumulate();
  if (factor < 0.25)
    factor = 0.25;
  if (factor > 4.0)
    factor = 4.0;
  g_speed = factor;
}

double simClockGetSpeed() { return g_speed; }

void simClockSetPaused(bool paused) {
  accumulate();
  g_paused = paused;
}

bool simClockIsPaused() { return g_paused; }

void simClockStep(unsigned long ms) {
  accumulate();
  g_simMs += static_cast<double>(ms);
}

unsigned long simClockNow() {
  accumulate();
  return static_cast<unsigned long>(g_simMs);
}
