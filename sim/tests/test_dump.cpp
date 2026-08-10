#include "test_main.h"

#include "Arduino.h"
#include "PluginManager.h"
#include "constants.h"
#include "dump.h"
#include "frame.h"
#include "screen.h"
#include "sim_registry.h"
#include <string>

static void test_dump_has_one_line_per_row() {
  Screen.setCurrentRotation(0);
  Screen.setBrightness(MAX_BRIGHTNESS);
  Screen.clear();
  simRenderTick();
  std::string d = simDumpFrame(simLatestFrame());
  int newlines = 0;
  for (char c : d)
    if (c == '\n')
      newlines++;
  CHECK_EQ(newlines, ROWS);
  CHECK_EQ((int)d.size(), ROWS * (COLS + 1));
}

static void test_dump_maps_off_and_full_brightness() {
  Screen.setCurrentRotation(0);
  Screen.setBrightness(MAX_BRIGHTNESS);
  Screen.clear();
  Screen.setPixel(0, 0, 1, MAX_BRIGHTNESS);
  simRenderTick();
  std::string d = simDumpFrame(simLatestFrame());
  CHECK_EQ(d[0], '@');
  CHECK_EQ(d[1], ' ');
}

static void test_checkerboard_has_alternating_neighbours() {
  simRegisterPlugins();
  Screen.setCurrentRotation(0);
  Screen.setBrightness(MAX_BRIGHTNESS);
  pluginManager.setActivePlugin("Checkerboard");
  pluginManager.setupActivePlugin();
  pluginManager.runActivePlugin();
  simRenderTick();
  SimFrame f = simLatestFrame();
  int differing = 0;
  for (int y = 0; y < ROWS; y++)
    for (int x = 0; x + 1 < COLS; x++)
      if (f.px[y * COLS + x] != f.px[y * COLS + x + 1])
        differing++;
  CHECK(differing > 0);
}

static void test_unknown_plugin_returns_error_code() {
  CHECK_EQ(simRunHeadless("NoSuchPlugin", 1), 2);
}

int main() {
  Screen.setup();
  RUN(test_dump_has_one_line_per_row);
  RUN(test_dump_maps_off_and_full_brightness);
  RUN(test_checkerboard_has_alternating_neighbours);
  RUN(test_unknown_plugin_returns_error_code);
  TEST_MAIN_END;
}
