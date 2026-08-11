#pragma once

#include "constants.h"
#include <Arduino.h>

/**
 * One weather reading, shared by every plugin that shows weather.
 *
 * Both WeatherPlugin and MatrixClockPlugin need the same forecast. Fetching it
 * twice would mean two requests to a free service and two update schedules that
 * can disagree on the same device, so the fetch and cache live here.
 */
struct WeatherReading
{
  int temperatureC = 0;
  int icon = 0; // index into weatherIcons

  // Moon, from the same response: no extra request needed.
  double moonIllumination = 0.0; // 0..1
  bool moonWaxing = true;

  bool valid = false;
};

class WeatherStore
{
private:
  WeatherStore() = default;

  WeatherReading reading_;
  unsigned long lastUpdate_ = 0;
  bool everFetched_ = false;

  bool fetch();

public:
  static WeatherStore &getInstance();

  WeatherStore(const WeatherStore &) = delete;
  WeatherStore &operator=(const WeatherStore &) = delete;

  /** Fetches at most every 30 minutes. Safe to call from a plugin loop. */
  void update();

  bool hasData() const
  {
    return reading_.valid;
  }

  WeatherReading get() const
  {
    return reading_;
  }
};

extern WeatherStore &weatherStore;
