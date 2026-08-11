#pragma once

#include "PluginManager.h"
#include "weather_store.h"

/**
 * Shows the current weather icon above the temperature.
 *
 * Fetching and caching live in WeatherStore, which MatrixClockPlugin shares,
 * so this plugin only renders. Layout comes from scene_builder, which measures
 * the icon rather than using a fixed position per condition.
 */
class WeatherPlugin : public Plugin
{
private:
  static constexpr uint8_t WEATHER_BRIGHTNESS = 100;

  bool hasDrawn = false;
  int drawnTemperature = 0;
  int drawnIcon = -1;

  void drawWeather();
  void drawLoadingScreen();

public:
  void setup() override;
  void loop() override;
  const char *getName() const override;
};
