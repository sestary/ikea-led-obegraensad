#pragma once

// Simulated time source backing millis(). Real monotonic time scaled by a
// speed factor, freezable for pause and advanceable by an explicit step.
void simClockSetSpeed(double factor);
double simClockGetSpeed();
void simClockSetPaused(bool paused);
bool simClockIsPaused();
void simClockStep(unsigned long ms);
unsigned long simClockNow();
