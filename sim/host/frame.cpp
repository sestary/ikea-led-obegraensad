#include "frame.h"

#include "screen.h"

namespace {
SimFrame g_frame{};
}

void simPublishFrame(const uint8_t *buf, uint8_t globalBrightness) {
  for (int i = 0; i < TOTAL_PIXELS; i++) {
    g_frame.px[i] = static_cast<uint8_t>(
        (static_cast<uint16_t>(buf[i]) * globalBrightness) / MAX_BRIGHTNESS);
  }
  g_frame.seq++;
}

SimFrame simLatestFrame() { return g_frame; }

void simRenderTick() { Screen.render(); }
