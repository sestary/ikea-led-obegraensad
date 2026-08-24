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
#include <vector>

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
  uint8_t mask[TOTAL_PIXELS];
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

// The time is set monospace: every digit advances by the widest of the ten
// (7px) with TIME_GAP blank columns between, so two cells and the gap come to
// exactly the panel's 16 and the digits never move as the time changes.
//
// That fixes the cells at [0,6] and [9,15], which leaves columns 7 and 8 clear
// whatever the digits are - an assertion proportional packing could not meet,
// since there the row width, and so every digit's position, moved with the
// digits being shown.
static constexpr int TIME_GAP = 2;
static constexpr int TIME_CELL = 7;
static void test_time_digits_are_monospaced() {
  const int times[][2] = {{11, 38}, {23, 59}, {10, 8}, {11, 11}, {0, 0}};

  for (const auto &t : times) {
    uint8_t mask[TOTAL_PIXELS];
    std::memset(mask, 0, sizeof(mask));
    buildTimeMask(mask, t[0], t[1]);

    for (int half = 0; half < 2; half++) {
      const int value = t[half];
      const int digits[2] = {value / 10, value % 10};
      const int cellStart[2] = {0, TIME_CELL + TIME_GAP};

      // The gap columns stay clear however the digits are set.
      for (int y = 0; y < ROWS; y++) {
        if ((y < ROWS / 2) != (half == 0))
          continue;
        for (int x = TIME_CELL; x < TIME_CELL + TIME_GAP; x++)
          CHECK_EQ(mask[y * COLS + x], 0);
      }

      // And each digit sits centred inside its own cell.
      for (int d = 0; d < 2; d++) {
        Glyph g = captureGlyph([&] { Screen.drawBigNumbers(0, 0, {digits[d]}); });

        int lo = COLS, hi = -1;
        for (int y = 0; y < ROWS; y++) {
          if ((y < ROWS / 2) != (half == 0))
            continue;
          for (int x = cellStart[d]; x < cellStart[d] + TIME_CELL; x++)
            if (mask[y * COLS + x]) {
              if (x < lo)
                lo = x;
              if (x > hi)
                hi = x;
            }
        }
        CHECK(hi >= 0);
        CHECK_EQ(hi - lo + 1, g.width);
        CHECK_EQ(lo, cellStart[d] + (TIME_CELL - g.width) / 2);
      }
    }
  }
}

// Every glyph must survive composition. drawCharacter writes its blanks as
// zeros, so overlapping glyphs used to erase each other.
static void test_negative_temperature_keeps_every_glyph() {
  uint8_t mask[TOTAL_PIXELS];
  std::memset(mask, 0, sizeof(mask));
  buildWeatherMask(mask, -12, 2);

  const std::vector<int> minusBits = {0x00, 0xC0, 0x00};
  Glyph minus = captureGlyph(
      [&] { Screen.drawCharacter(0, 0, Screen.readBytes(minusBits), 4, MAX_BRIGHTNESS); });
  Glyph one = captureGlyph([&] { Screen.drawNumbers(0, 0, {1}); });
  Glyph two = captureGlyph([&] { Screen.drawNumbers(0, 0, {2}); });
  Glyph degree = captureGlyph([&] {
    Screen.drawCharacter(0, 0, Screen.readBytes(degreeSymbol), 4, MAX_BRIGHTNESS);
  });
  const int expected =
      (int)(minus.px.size() + one.px.size() + two.px.size() + degree.px.size());

  // The temperature occupies the lowest lit rows; count only those, since the
  // icon above is procedural and anti-aliased.
  int bottom = -1;
  for (int i = 0; i < TOTAL_PIXELS; i++)
    if (mask[i] > 0)
      bottom = i / COLS;

  int present = 0;
  for (int y = bottom - 4; y <= bottom; y++)
    for (int x = 0; x < COLS; x++)
      if (y >= 0 && mask[y * COLS + x] > 0)
        present++;

  CHECK_EQ(present, expected);
}

static void test_weather_mask_keeps_two_row_gap_for_every_icon() {
  for (int icon = 0; icon < 7; icon++) {
    uint8_t mask[TOTAL_PIXELS];
    std::memset(mask, 0, sizeof(mask));
    buildWeatherMask(mask, 21, icon);

    bool inked[ROWS] = {false};
    for (int y = 0; y < ROWS; y++)
      for (int x = 0; x < COLS; x++)
        if (mask[y * COLS + x] > 0)
          inked[y] = true;

    int bottom = -1;
    for (int y = 0; y < ROWS; y++)
      if (inked[y])
        bottom = y;
    CHECK(bottom >= 0);

    // The temperature is the lowest contiguous run of inked rows. Count the
    // blank rows directly above it: the icons themselves have internal gaps
    // (fog is three separate bars), so counting every blank row would not
    // measure the separation.
    int tempTop = bottom;
    while (tempTop > 0 && inked[tempTop - 1])
      tempTop--;

    int blanks = 0;
    int y = tempTop - 1;
    while (y >= 0 && !inked[y]) {
      blanks++;
      y--;
    }

    CHECK(y >= 0); // there is artwork above the temperature
    CHECK_EQ(blanks, 2);
  }
}

static void test_temperature_is_centred() {
  uint8_t mask[TOTAL_PIXELS];
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

// A stream might never cross a given pixel, so the scene completes on a
// deadline as well as on every target locking.
static void test_scene_completes_by_the_deadline() {
  Plugin *p = matrixClock();
  CHECK(p != nullptr);
  Screen.setCurrentRotation(0);
  Screen.setBrightness(MAX_BRIGHTNESS);
  pluginManager.setActivePluginById(p->getId());
  pluginManager.setupActivePlugin();

  // Captured now, not after the loop: the scene's target is built once when the
  // scene starts, and the rain-in window is long enough for the minute to roll
  // underneath it.
  uint8_t mask[TOTAL_PIXELS];
  std::memset(mask, 0, sizeof(mask));
  struct tm t;
  getLocalTime(&t);
  buildTimeMask(mask, t.tm_hour, t.tm_min);

  // Rain-in deadline, ticking at 50ms.
  for (int i = 0; i < 80; i++) {
    pluginManager.runActivePlugin();
    simClockStep(50);
  }
  pluginManager.runActivePlugin();
  simRenderTick();

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

// The streams must drain off the bottom rather than blink out mid-panel. The
// hold hides the rain, and cutting it the instant the image finished locking
// left a full screen of rain vanishing in one frame.
//
// Measured as the rain's pixel count in the last frame that had any: draining
// leaves a thinning tail, whereas cutting it strands a full panel. The lowest
// lit row is no use here - with every column raining, something is near the
// bottom in almost any frame.
static void test_rain_drains_instead_of_vanishing() {
  Plugin *p = matrixClock();
  pluginManager.setActivePluginById(p->getId());
  pluginManager.setupActivePlugin();

  bool sawRain = false;
  int lastRainCount = -1;
  int peakRainCount = 0;
  int rainAtCut = -1;

  for (int i = 0; i < 400; i++) {
    pluginManager.runActivePlugin();
    simRenderTick();
    SimFrame f = simLatestFrame();

    // The time scene draws at full brightness, so anything at or below the
    // rain's ceiling is rain.
    int rain = 0;
    for (int j = 0; j < TOTAL_PIXELS; j++) {
      const uint8_t v = f.px[j];
      if (v > 0 && v <= 90)
        rain++;
    }

    if (rain > 0) {
      sawRain = true;
      lastRainCount = rain;
      if (rain > peakRainCount)
        peakRainCount = rain;
    } else if (sawRain) {
      rainAtCut = lastRainCount;
      break;
    }

    simClockStep(50);
  }

  CHECK(sawRain);
  CHECK(rainAtCut >= 0);
  CHECK(peakRainCount > 20);           // the rain really did fill the panel
  CHECK(rainAtCut * 4 < peakRainCount); // and had thinned right out before it went
}

static void test_an_interlude_follows_the_clock() {
  simSetWeather(21, 113);
  Plugin *p = matrixClock();
  pluginManager.setActivePluginById(p->getId());
  pluginManager.setupActivePlugin();

  uint8_t weatherMask[TOTAL_PIXELS];
  std::memset(weatherMask, 0, sizeof(weatherMask));
  buildWeatherMask(weatherMask, 21, 2);

  // The weather takes over at :45, so a full minute of ticks must show it.
  bool sawInterlude = false;
  for (int i = 0; i < 2000 && !sawInterlude; i++) {
    pluginManager.runActivePlugin();
    simRenderTick();
    SimFrame f = simLatestFrame();
    int hits = 0, need = 0;
    for (int j = 0; j < TOTAL_PIXELS; j++)
      if (weatherMask[j] > 0) {
        need++;
        if (f.px[j] == weatherMask[j])
          hits++;
      }
    if (need > 0 && hits == need)
      sawInterlude = true;
    simClockStep(50);
  }
  CHECK(sawInterlude);
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

// Captures exactly the first scene's dissolve: the frames after the target has
// been fully assembled and is coming apart again, stopping before the next
// scene starts. Anchoring on observed state rather than tick counts keeps this
// robust to the rain-in finishing early.
static std::vector<SimFrame> collectFirstDissolve(const uint8_t *mask) {
  Plugin *p = matrixClock();
  pluginManager.setActivePluginById(p->getId());
  pluginManager.setupActivePlugin();
  Screen.setBrightness(MAX_BRIGHTNESS);
  Screen.setCurrentRotation(0);

  int total = 0;
  for (int i = 0; i < TOTAL_PIXELS; i++)
    if (mask[i])
      total++;

  std::vector<SimFrame> frames;
  bool peaked = false;
  // rain-in 2.5s + clock hold 30s + dissolve 1.2s, at 50ms a tick
  for (int i = 0; i < 900; i++) {
    pluginManager.runActivePlugin();
    simRenderTick();
    SimFrame f = simLatestFrame();

    int onTarget = 0;
    for (int j = 0; j < TOTAL_PIXELS; j++)
      if (mask[j] && f.px[j] == MAX_BRIGHTNESS)
        onTarget++;

    if (!peaked && total > 0 && onTarget == total) {
      peaked = true;
    } else if (peaked && onTarget < total) {
      if (onTarget == 0)
        break; // scene finished; anything later belongs to the next scene
      frames.push_back(f);
    }
    simClockStep(50);
  }
  return frames;
}

static void timeMaskNow(uint8_t *mask) {
  std::memset(mask, 0, TOTAL_PIXELS);
  struct tm t;
  getLocalTime(&t);
  buildTimeMask(mask, t.tm_hour, t.tm_min);
}

// Released pixels must stay bright and travel, so they light positions the
// target never lit. Painting them at rain brightness made the image look
// erased rather than falling.
static void test_dissolve_drops_fall_visibly() {
  uint8_t mask[TOTAL_PIXELS];
  timeMaskNow(mask);
  std::vector<SimFrame> frames = collectFirstDissolve(mask);
  CHECK(!frames.empty());

  // Background rain tops out at 90. A falling pixel starts near full and fades,
  // so anything well above the rain at a position the target never lit must be
  // a released pixel in flight.
  const int RAIN_CEILING = 120;
  int brightOffTarget = 0;
  for (const SimFrame &f : frames)
    for (int j = 0; j < TOTAL_PIXELS; j++)
      if (!mask[j] && f.px[j] > RAIN_CEILING)
        brightOffTarget++;

  CHECK(brightOffTarget > 0);
}

// Releasing a whole row at once reads as erasure. Within the dissolve there
// must be a row where some target pixels have let go and others have not.
static void test_dissolve_crumbles_rather_than_erasing_rows() {
  uint8_t mask[TOTAL_PIXELS];
  timeMaskNow(mask);
  std::vector<SimFrame> frames = collectFirstDissolve(mask);
  CHECK(!frames.empty());

  bool sawPartialRow = false;
  for (const SimFrame &f : frames) {
    for (int y = 0; y < ROWS && !sawPartialRow; y++) {
      int held = 0, released = 0;
      for (int x = 0; x < COLS; x++) {
        const int j = y * COLS + x;
        if (!mask[j])
          continue; // only target pixels can be held or released
        if (f.px[j] == MAX_BRIGHTNESS)
          held++;
        else
          released++;
      }
      if (held > 0 && released > 0)
        sawPartialRow = true;
    }
    if (sawPartialRow)
      break;
  }
  CHECK(sawPartialRow);
}

// The clock is the resident screen: interludes are brief and never follow one
// another, so the time must dominate a long run.
static void test_time_dominates_the_cycle() {
  simSetWeather(21, 113);
  Plugin *p = matrixClock();
  pluginManager.setActivePluginById(p->getId());
  pluginManager.setupActivePlugin();

  // Align to the top of a minute. Scenes are pinned to the wall clock now, and
  // one captured mask is only valid while the displayed minute holds - so the
  // window has to be a single minute, measured from its start.
  struct tm t;
  for (int i = 0; i < 1200; i++) {
    getLocalTime(&t);
    if (t.tm_sec == 0)
      break;
    pluginManager.runActivePlugin();
    simClockStep(50);
  }
  CHECK_EQ(t.tm_sec, 0);

  uint8_t timeMask[TOTAL_PIXELS];
  timeMaskNow(timeMask);
  int timeTotal = 0;
  for (int i = 0; i < TOTAL_PIXELS; i++)
    if (timeMask[i] > 0)
      timeTotal++;

  int timeFrames = 0, frames = 0;
  // 59 seconds at 50ms a tick: the whole minute bar the roll into the next.
  for (int i = 0; i < 1180; i++) {
    pluginManager.runActivePlugin();
    simRenderTick();
    SimFrame f = simLatestFrame();
    int hits = 0;
    for (int j = 0; j < TOTAL_PIXELS; j++)
      if (timeMask[j] > 0 && f.px[j] == timeMask[j])
        hits++;
    if (timeTotal > 0 && hits == timeTotal)
      timeFrames++;
    frames++;
    simClockStep(50);
  }
  CHECK(frames > 0);
  CHECK(timeFrames * 2 > frames); // the clock is up more than half the time
}

// The button steps the screens in order, so it is predictable by hand even
// though the automatic picker is weighted and random.
static void test_button_steps_through_the_screens() {
  simSetWeather(21, 113);
  Plugin *p = matrixClock();
  pluginManager.setActivePluginById(p->getId());
  pluginManager.setupActivePlugin();

  uint8_t weatherMask[TOTAL_PIXELS];
  std::memset(weatherMask, 0, sizeof(weatherMask));
  buildWeatherMask(weatherMask, 21, 2);

  CHECK(p->buttonPressed()); // consumed, so the lamp does not change plugin

  // Let the weather rain in, then it must be the weather that assembled.
  bool sawWeather = false;
  for (int i = 0; i < 80 && !sawWeather; i++) {
    pluginManager.runActivePlugin();
    simRenderTick();
    SimFrame f = simLatestFrame();
    int hits = 0, need = 0;
    for (int j = 0; j < TOTAL_PIXELS; j++)
      if (weatherMask[j] > 0) {
        need++;
        if (f.px[j] == weatherMask[j])
          hits++;
      }
    if (need > 0 && hits == need)
      sawWeather = true;
    simClockStep(50);
  }
  CHECK(sawWeather);
}

// A plugin that does not use the button must not swallow it, or the lamp
// could never be changed by hand.
static void test_other_plugins_do_not_consume_the_button() {
  simRegisterPlugins();
  for (Plugin *p : pluginManager.getAllPlugins())
    if (std::string("Matrix Clock") != p->getName())
      CHECK(!p->buttonPressed());
}

int main() {
  Screen.setup();
  RUN(test_time_mask_is_flush_top_and_bottom);
  RUN(test_time_digits_are_monospaced);
  RUN(test_negative_temperature_keeps_every_glyph);
  RUN(test_weather_mask_keeps_two_row_gap_for_every_icon);
  RUN(test_temperature_is_centred);
  RUN(test_scene_completes_by_the_deadline);
  RUN(test_rain_drains_instead_of_vanishing);
  RUN(test_an_interlude_follows_the_clock);
  RUN(test_runs_without_weather_data);
  RUN(test_time_dominates_the_cycle);
  RUN(test_button_steps_through_the_screens);
  RUN(test_other_plugins_do_not_consume_the_button);
  RUN(test_dissolve_drops_fall_visibly);
  RUN(test_dissolve_crumbles_rather_than_erasing_rows);
  TEST_MAIN_END;
}
