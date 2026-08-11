#include "test_main.h"

#include "Arduino.h"
#include "PluginManager.h"
#include "constants.h"
#include "frame.h"
#include "scene_builder.h"
#include "screen.h"
#include "sim_clock.h"
#include "sim_registry.h"
#include "sim_weather.h"
#include <cstring>
#include <string>

static Plugin *matrixClock() {
  simRegisterPlugins();
  for (Plugin *p : pluginManager.getAllPlugins())
    if (std::string("Matrix Clock") == p->getName())
      return p;
  return nullptr;
}

static int litCount(const SimFrame &f) {
  int n = 0;
  for (int i = 0; i < TOTAL_PIXELS; i++)
    if (f.px[i])
      n++;
  return n;
}

// --- scene_builder ---------------------------------------------------------

static void test_time_mask_is_flush_top_and_bottom() {
  bool mask[TOTAL_PIXELS];
  std::memset(mask, 0, sizeof(mask));
  buildTimeMask(mask, 14, 32);

  bool topRow = false, bottomRow = false;
  for (int x = 0; x < COLS; x++) {
    if (mask[x])
      topRow = true;
    if (mask[(ROWS - 1) * COLS + x])
      bottomRow = true;
  }
  CHECK(topRow);    // hours reach row 0
  CHECK(bottomRow); // minutes reach row 15
}

// Every glyph must survive composition. drawCharacter writes its blanks as
// zeros, so overlapping glyphs used to erase each other.
static void test_negative_temperature_keeps_every_glyph() {
  bool mask[TOTAL_PIXELS];
  std::memset(mask, 0, sizeof(mask));
  buildWeatherMask(mask, -12, 2);

  // Count lit pixels on the temperature rows; compare against the glyphs drawn
  // in isolation. Any clipping shows up as a shortfall.
  Glyph minus, one, two, degree;
  {
    const std::vector<int> m = {0x00, 0xC0, 0x00};
    minus = captureGlyph(
        [&] { Screen.drawCharacter(0, 0, Screen.readBytes(m), 4, MAX_BRIGHTNESS); });
    one = captureGlyph([&] { Screen.drawNumbers(0, 0, {1}); });
    two = captureGlyph([&] { Screen.drawNumbers(0, 0, {2}); });
    degree = captureGlyph([&] {
      Screen.drawCharacter(0, 0, Screen.readBytes(degreeSymbol), 4, MAX_BRIGHTNESS);
    });
  }
  const int expected = (int)(minus.px.size() + one.px.size() + two.px.size() + degree.px.size());

  int total = 0;
  for (int i = 0; i < TOTAL_PIXELS; i++)
    if (mask[i])
      total++;

  bool iconMask[TOTAL_PIXELS];
  std::memset(iconMask, 0, sizeof(iconMask));
  Glyph icon = captureGlyph([&] { Screen.drawWeather(0, 0, 2, MAX_BRIGHTNESS); });

  CHECK_EQ(total - (int)icon.px.size(), expected);
}

static void test_weather_mask_keeps_two_row_gap_for_every_icon() {
  for (int icon = 0; icon < 7; icon++) {
    bool mask[TOTAL_PIXELS];
    std::memset(mask, 0, sizeof(mask));
    buildWeatherMask(mask, 21, icon);

    // The mask is two blocks — icon above, temperature below — so the blank
    // rows between the topmost and bottommost ink are exactly the gap.
    bool inked[ROWS] = {false};
    for (int y = 0; y < ROWS; y++)
      for (int x = 0; x < COLS; x++)
        if (mask[y * COLS + x])
          inked[y] = true;

    int first = -1, last = -1;
    for (int y = 0; y < ROWS; y++)
      if (inked[y]) {
        if (first < 0)
          first = y;
        last = y;
      }

    int blanks = 0;
    for (int y = first; y <= last; y++)
      if (!inked[y])
        blanks++;

    CHECK(first >= 0);
    CHECK_EQ(blanks, 2);
  }
}

static void test_temperature_is_centred() {
  bool mask[TOTAL_PIXELS];
  std::memset(mask, 0, sizeof(mask));
  buildWeatherMask(mask, 21, 2);

  // Temperature occupies the lowest lit rows.
  int bottom = -1;
  for (int i = 0; i < TOTAL_PIXELS; i++)
    if (mask[i])
      bottom = i / COLS;

  int lo = COLS, hi = -1;
  for (int y = bottom - 4; y <= bottom; y++)
    for (int x = 0; x < COLS; x++)
      if (y >= 0 && mask[y * COLS + x]) {
        if (x < lo)
          lo = x;
        if (x > hi)
          hi = x;
      }
  const int leftMargin = lo;
  const int rightMargin = COLS - 1 - hi;
  // Centred, allowing one pixel where the leftover space is odd.
  CHECK(abs(leftMargin - rightMargin) <= 1);
}

// --- plugin ----------------------------------------------------------------

// The deadline guarantee: however the rain falls, the scene must complete.
static void test_scene_completes_by_the_deadline() {
  Plugin *p = matrixClock();
  CHECK(p != nullptr);
  Screen.setCurrentRotation(0);
  Screen.setBrightness(MAX_BRIGHTNESS);
  pluginManager.setActivePluginById(p->getId());
  pluginManager.setupActivePlugin();

  // 2500ms rain-in deadline, ticking at 50ms.
  for (int i = 0; i < 80; i++) {
    pluginManager.runActivePlugin();
    simClockStep(50);
  }
  pluginManager.runActivePlugin();
  simRenderTick();

  bool mask[TOTAL_PIXELS];
  std::memset(mask, 0, sizeof(mask));
  struct tm t;
  getLocalTime(&t);
  buildTimeMask(mask, t.tm_hour, t.tm_min);

  SimFrame f = simLatestFrame();
  int expected = 0, present = 0;
  for (int i = 0; i < TOTAL_PIXELS; i++)
    if (mask[i]) {
      expected++;
      if (f.px[i] == MAX_BRIGHTNESS)
        present++;
    }
  CHECK(expected > 0);
  CHECK_EQ(present, expected);
}

static void test_cycle_reaches_the_weather_scene() {
  simSetWeather(21, 113);
  Plugin *p = matrixClock();
  pluginManager.setActivePluginById(p->getId());
  pluginManager.setupActivePlugin();

  bool weatherMask[TOTAL_PIXELS];
  std::memset(weatherMask, 0, sizeof(weatherMask));
  buildWeatherMask(weatherMask, 21, 2);

  bool sawWeather = false;
  // One full scene is 2500 + 8000 + 1200 ms; run two of them.
  for (int i = 0; i < 600 && !sawWeather; i++) {
    pluginManager.runActivePlugin();
    simRenderTick();
    SimFrame f = simLatestFrame();
    int hits = 0, need = 0;
    for (int j = 0; j < TOTAL_PIXELS; j++)
      if (weatherMask[j]) {
        need++;
        if (f.px[j] == MAX_BRIGHTNESS)
          hits++;
      }
    if (need > 0 && hits == need)
      sawWeather = true;
    simClockStep(50);
  }
  CHECK(sawWeather);
}

static void test_runs_without_weather_data() {
  // Nothing should stall when no reading exists; the time scene still runs.
  Plugin *p = matrixClock();
  pluginManager.setActivePluginById(p->getId());
  pluginManager.setupActivePlugin();
  for (int i = 0; i < 400; i++) {
    pluginManager.runActivePlugin();
    simRenderTick();
    simClockStep(50);
  }
  CHECK(litCount(simLatestFrame()) > 0);
}

int main() {
  Screen.setup();
  RUN(test_time_mask_is_flush_top_and_bottom);
  RUN(test_negative_temperature_keeps_every_glyph);
  RUN(test_weather_mask_keeps_two_row_gap_for_every_icon);
  RUN(test_temperature_is_centred);
  RUN(test_scene_completes_by_the_deadline);
  RUN(test_cycle_reaches_the_weather_scene);
  RUN(test_runs_without_weather_data);
  TEST_MAIN_END;
}
