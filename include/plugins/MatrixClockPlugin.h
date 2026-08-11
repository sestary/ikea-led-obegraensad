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
    SCENE_COUNT
  };

  static constexpr uint16_t TICK_MS = 50;
  static constexpr uint16_t RAIN_IN_MAX_MS = 2500;
  static constexpr uint16_t HOLD_MS = 8000;
  static constexpr uint16_t DISSOLVE_MS = 1200;

  static constexpr uint8_t RAIN_BRIGHTNESS = 90;
  static constexpr uint8_t MAX_TRAIL_LENGTH = 6;

  struct Column
  {
    int8_t y;
    int8_t previousY;
    uint8_t speed;
    uint8_t length;
  };

  struct Drop
  {
    int8_t x;
    int8_t y;
  };

  NonBlockingDelay timer;
  Column columns[COLS];
  bool target[TOTAL_PIXELS];
  bool locked[TOTAL_PIXELS];
  std::vector<Drop> drops;

  Phase phase;
  int scene;
  unsigned long phaseStart;

  void startScene(int nextScene);
  void buildTarget();
  void resetColumn(int index, bool startAbove);
  void advanceRain();
  void lockCrossedPixels();
  bool allTargetsLocked() const;
  void lockEverything();
  void releaseLocked(unsigned long elapsed);
  void advanceDrops();
  void paint();

public:
  void setup() override;
  void loop() override;
  const char *getName() const override;
};
