#pragma once

#include "frame.h"
#include <cstdint>
#include <string>

// Two LED rows per text row: foreground is the upper LED, background the
// lower, so pixels come out roughly square.
std::string simRenderMatrix(const SimFrame &f);

std::string simRenderStatus(const char *plugin, int rotation,
                            uint8_t brightness, double fps, double speed);

std::string simRenderSidebar(int selected, int firstVisible, int visibleRows);
