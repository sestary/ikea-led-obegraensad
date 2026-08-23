#pragma once

#include "frame.h"
#include <string>

// ROWS lines of COLS characters, brightness bucketed onto " .:-=+*#%@".
std::string simDumpFrame(const SimFrame &f);

// Runs one plugin headlessly at the given rotation (0-3) and prints `frames`
// dumps. Returns 0 on success, 2 if the plugin name is not registered.
int simRunHeadless(const std::string &plugin, int frames, int rotation = 0);
