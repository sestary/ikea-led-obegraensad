#include "plugins/WeatherPlugin.h"
#include "scene_builder.h"

#include <cstring>

void WeatherPlugin::setup()
{
  Screen.clear();
  hasDrawn = false;
  drawnIcon = -1;

  if (weatherStore.hasData())
  {
    drawWeather();
  }
  else
  {
    drawLoadingScreen();
  }
}

void WeatherPlugin::loop()
{
  weatherStore.update();

  if (!weatherStore.hasData())
  {
    return;
  }

  const WeatherReading reading = weatherStore.get();
  if (hasDrawn && reading.icon == drawnIcon && reading.temperatureC == drawnTemperature)
  {
    return;
  }

  drawWeather();
}

void WeatherPlugin::drawWeather()
{
  const WeatherReading reading = weatherStore.get();

  uint8_t mask[TOTAL_PIXELS];
  std::memset(mask, 0, sizeof(mask));
  buildWeatherMask(mask, reading.temperatureC, reading.icon);

  Screen.clear();
  paintMask(mask, WEATHER_BRIGHTNESS);

  hasDrawn = true;
  drawnIcon = reading.icon;
  drawnTemperature = reading.temperatureC;
}

void WeatherPlugin::drawLoadingScreen()
{
  currentStatus = LOADING;

  Screen.setPixel(4, 7, 1);
  Screen.setPixel(5, 7, 1);
  Screen.setPixel(7, 7, 1);
  Screen.setPixel(8, 7, 1);
  Screen.setPixel(10, 7, 1);
  Screen.setPixel(11, 7, 1);

  currentStatus = NONE;
}

const char *WeatherPlugin::getName() const
{
  return "Weather";
}
