#include "app.h"

#include "Arduino.h"
#include "PluginManager.h"
#include "constants.h"
#include "frame.h"
#include "input.h"
#include "render.h"
#include "screen.h"
#include "sim_clock.h"
#include "sim_registry.h"
#include "sim_weather.h"
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

namespace {
const int kBrightnessStep = 8;
const int kStepMs = 33;
} // namespace

void simAppHandleKey(SimAppState &s, char key) {
  int count = static_cast<int>(pluginManager.getNumPlugins());

  switch (key) {
  case 'j':
    if (count > 0 && s.selected < count - 1)
      s.selected++;
    break;
  case 'k':
    if (s.selected > 0)
      s.selected--;
    break;
  case 'r':
    s.rotation = (s.rotation + 1) & 0x3;
    break;
  case '+':
  case '=':
    s.brightness = (s.brightness > MAX_BRIGHTNESS - kBrightnessStep)
                       ? MAX_BRIGHTNESS
                       : static_cast<uint8_t>(s.brightness + kBrightnessStep);
    break;
  case '-':
    s.brightness = (s.brightness < kBrightnessStep)
                       ? 0
                       : static_cast<uint8_t>(s.brightness - kBrightnessStep);
    break;
  case ' ':
    s.paused = !s.paused;
    simClockSetPaused(s.paused);
    break;
  case 's':
    simClockStep(kStepMs);
    break;
  case ']':
    simClockSetSpeed(simClockGetSpeed() * 2.0);
    break;
  case '[':
    simClockSetSpeed(simClockGetSpeed() / 2.0);
    break;
  case 'w':
    simCycleWeather();
    break;
  case 'b': {
    // The lamp's button. Plugins that show several screens step through them.
    Plugin *active = pluginManager.getActivePlugin();
    if (active != nullptr)
    {
      active->buttonPressed();
    }
    break;
  }
  case 'q':
    s.quit = true;
    break;
  default:
    break;
  }
}

int simRunApp() {
  simRegisterPlugins();
  simInputBegin();

  SimAppState s{0, 0, MAX_BRIGHTNESS, false, false};
  int active = -1;
  double fps = 0.0;
  bool firstFrame = true;
  auto lastFrame = std::chrono::steady_clock::now();

  while (!s.quit) {
    for (char k = simInputPoll(); k != 0; k = simInputPoll())
      simAppHandleKey(s, k);

    auto &plugins = pluginManager.getAllPlugins();
    if (s.selected != active && !plugins.empty()) {
      pluginManager.setActivePluginById(plugins[s.selected]->getId());
      pluginManager.setupActivePlugin();
      active = s.selected;
    }

    Screen.setCurrentRotation(s.rotation);
    Screen.setBrightness(s.brightness);

    pluginManager.runActivePlugin();
    simRenderTick();

    // The first iteration's delta is measured from before the loop started, so
    // it is meaninglessly small. Skip it rather than seeding fps from noise.
    auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - lastFrame).count();
    lastFrame = now;
    if (dt > 0) {
      if (firstFrame)
        firstFrame = false;
      else
        fps = (fps == 0.0) ? 1.0 / dt : 0.9 * fps + 0.1 * (1.0 / dt);
    }

    Plugin *activePlugin = pluginManager.getActivePlugin();
    const char *name = activePlugin ? activePlugin->getName() : "-";

    std::string out = "\033[H\033[2J";
    out += " IKEA OBEGRÄNSAD — simulator\n\n";
    out += simRenderMatrix(simLatestFrame());
    out += "\n ";
    out += simRenderStatus(name, s.rotation * 90, s.brightness, fps,
                           simClockGetSpeed());
    if (s.paused)
      out += "   [paused]";
    out += "\n\n";
    out += simRenderSidebar(s.selected, 0,
                            static_cast<int>(pluginManager.getNumPlugins()));
    out += "\n j/k plugin  r rotate  +/- bright  space pause  s step  "
           "[/] speed  w weather  b button  q quit\n";

    std::fwrite(out.data(), 1, out.size(), stdout);
    std::fflush(stdout);

    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  simInputEnd();
  return 0;
}
