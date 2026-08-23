#pragma once

#include <cstdint>

struct SimAppState {
  int selected;
  int rotation;
  uint8_t brightness;
  bool paused;
  bool quit;
};

// Pure state transition, so key handling is testable without a terminal.
void simAppHandleKey(SimAppState &s, char key);

// Interactive loop. Returns the process exit code.
int simRunApp();
