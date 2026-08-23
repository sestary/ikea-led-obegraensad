#pragma once

#include "constants.h"
#include <cstdint>

struct SimFrame {
  uint8_t px[TOTAL_PIXELS];
  unsigned long seq;
};

// Called by Screen_::_render() under -DSIMULATOR, in place of the SPI write.
// `buf` is already rotated by the firmware's own rotate().
void simPublishFrame(const uint8_t *buf, uint8_t globalBrightness);

// Copy of the most recently published frame.
SimFrame simLatestFrame();

// Drive one render pass. Used by the frame loop and by tests.
void simRenderTick();
