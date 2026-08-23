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
    PHASE_HOLD,
    PHASE_DISSOLVE
  };

  enum Scene
  {
    SCENE_TIME,
    SCENE_WEATHER,
    SCENE_MOON,
    SCENE_COUNT
  };

  // The rain advances one row per tick, so TICK_MS is what the falling speed
  // is actually made of. The two deadlines below are wall clock, so they scale
  // with it: leave them where they were and a slower tick would simply run out
  // of time, snapping the rest of the mask on instead of raining it in.
  static constexpr uint16_t TICK_MS = 75;
  static constexpr uint16_t RAIN_IN_MAX_MS = 3750;
  static constexpr uint16_t DISSOLVE_MS = 1800;

  // The clock is the resident screen; weather and the moon are interludes. At
  // these holds the time is up roughly four fifths of the time.
  static constexpr uint32_t HOLD_TIME_MS = 30000;
  static constexpr uint32_t HOLD_INTERLUDE_MS = 8000;

  // Out of ten interludes, how many are weather rather than the moon.
  static constexpr int WEATHER_SHARE = 7;

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
  uint32_t holdDuration() const;
  void buildTarget();
  void resetColumn(int index, bool startAbove);
  void advanceRain();
  void lockCrossedPixels();
  bool allTargetsLocked() const;
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
