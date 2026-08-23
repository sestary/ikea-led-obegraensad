#include "dump.h"

#include "PluginManager.h"
#include "constants.h"
#include "screen.h"
#include "sim_clock.h"
#include "sim_registry.h"
#include <cstdio>

namespace {
const char kRamp[] = " .:-=+*#%@";
// Highest index into kRamp, excluding the terminating NUL.
const int kRampMax = static_cast<int>(sizeof(kRamp)) - 2;
} // namespace

std::string simDumpFrame(const SimFrame &f) {
  std::string out;
  out.reserve(ROWS * (COLS + 1));
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      int v = f.px[y * COLS + x];
      out += kRamp[(v * kRampMax) / MAX_BRIGHTNESS];
    }
    out += '\n';
  }
  return out;
}

int simRunHeadless(const std::string &plugin, int frames, int rotation) {
  simRegisterPlugins();
  Screen.setCurrentRotation(rotation);

  Plugin *found = nullptr;
  for (Plugin *p : pluginManager.getAllPlugins())
    if (plugin == p->getName())
      found = p;

  if (!found) {
    std::fprintf(stderr, "unknown plugin: %s\navailable:\n", plugin.c_str());
    for (Plugin *p : pluginManager.getAllPlugins())
      std::fprintf(stderr, "  %s\n", p->getName());
    return 2;
  }

  pluginManager.setActivePluginById(found->getId());
  pluginManager.setupActivePlugin();

  for (int i = 0; i < frames; i++) {
    pluginManager.runActivePlugin();
    simRenderTick();
    std::printf("frame %d\n%s\n", i, simDumpFrame(simLatestFrame()).c_str());
    simClockStep(33);
  }
  return 0;
}
