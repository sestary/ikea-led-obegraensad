#pragma once

#include "PluginManager.h"
#include "timing.h"
#include <vector>

/**
 * Matrix-style falling blocks that condense into the time, dissolve, and
 * rebuild as the weather.
 *
 * Each lit block is a "letter", the same vocabulary as MatrixRainPlugin. The
 * target images come from the firmware's own drawing helpers via
 * scene_builder, so the digits and icons match the rest of the firmware.
 *
 * The rain code is deliberately not shared with MatrixRainPlugin: this one
 * renders dim, locks pixels onto a target and dissolves, which that plugin has
 * no use for.
 */
class MatrixClockPlugin : public Plugin
{
private:
  enum Phase
  {
    PHASE_RAIN_IN,
    // The image is complete but the rain is still on screen. Cutting it the
    // instant the last pixel locked left the streams vanishing in mid-fall, so
    // they run off the bottom edge first and the panel is clean before it holds.
    PHASE_DRAIN,
    PHASE_HOLD,
    PHASE_DISSOLVE
  };

  enum Scene
  {
    SCENE_TIME,
    SCENE_WEATHER,
    SCENE_COUNT
  };

  // The rain advances one row per tick, so TICK_MS is what the falling speed
  // is actually made of. The two deadlines below are wall clock, so they scale
  // with it: leave them where they were and a slower tick would simply run out
  // of time, snapping the rest of the mask on instead of raining it in.
  static constexpr uint16_t TICK_MS = 75;
  static constexpr uint16_t RAIN_IN_MAX_MS = 3750;
  static constexpr uint16_t DISSOLVE_MS = 1800;

  // Long enough for the slowest stream to clear a full panel height, with room
  // to spare: a backstop, not the usual way out of the drain.
  static constexpr uint16_t DRAIN_MAX_MS = 2000;

  // Scene changes are pinned to the wall clock rather than free-running: the
  // time holds from :00, hands over to the weather at :45, and comes back on
  // the minute. So the clock is always correct the instant it reassembles,
  // instead of drifting into view at some arbitrary point in the minute.
  static constexpr int WEATHER_AT_SECOND = 45;

  // A scene that finished assembling just past a boundary would otherwise
  // dissolve again immediately, which reads as a glitch rather than a change.
  static constexpr uint32_t MIN_HOLD_MS = 1500;

  // A press asks for the weather now, so it has to survive the schedule: the
  // wall clock would otherwise pull the panel straight back to the time, since
  // outside :45 to :00 the weather's window is not open.
  static constexpr uint32_t BUTTON_HOLD_MS = 10000;

  // Fallbacks for a device that has not reached an NTP server yet: with no
  // wall second to pin to, the scenes just take turns.
  static constexpr uint32_t HOLD_TIME_MS = 30000;
  static constexpr uint32_t HOLD_INTERLUDE_MS = 8000;

  // Set by a press, cleared whenever a scene starts.
  bool buttonHeld = false;

  static constexpr uint8_t RAIN_BRIGHTNESS = 90;
  static constexpr uint8_t MAX_TRAIL_LENGTH = 6;

  struct Column
  {
    int8_t y;
    int8_t previousY;
    uint8_t speed;
    uint8_t length;
  };

  // A pixel that has come loose from the image and is falling away. It keeps
  // its brightness so the image visibly breaks apart, fading into the rain.
  struct Drop
  {
    int8_t x;
    int8_t y;
    uint8_t brightness;
  };

  // How much a falling pixel dims per tick.
  static constexpr uint8_t DROP_FADE = 22;

  NonBlockingDelay timer;
  Column columns[COLS];
  // The target image is 8-bit so shaded artwork survives the rain: a locked
  // pixel lights at its own value, not a flat full brightness.
  uint8_t target[TOTAL_PIXELS];
  bool locked[TOTAL_PIXELS];
  // When each locked pixel lets go, in ms from the start of the dissolve.
  uint16_t releaseAt[TOTAL_PIXELS];
  std::vector<Drop> drops;

  Phase phase;
  int scene;
  unsigned long phaseStart;

  void startScene(int nextScene);
  int chooseNextScene() const;
  bool holdComplete(unsigned long elapsed) const;
  void buildTarget();
  void resetColumn(int index, bool startAbove);
  void advanceRain();
  void lockCrossedPixels();
  bool allTargetsLocked() const;
  bool rainDrained() const;
  void lockEverything();
  void scheduleDissolve();
  void releaseLocked(unsigned long elapsed);
  void advanceDrops();
  void paint();

public:
  void setup() override;
  void loop() override;
  bool buttonPressed() override;
  const char *getName() const override;
};
