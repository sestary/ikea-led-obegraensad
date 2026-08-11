#include "plugins/MatrixClockPlugin.h"
#include "scene_builder.h"
#include "weather_store.h"

#include <cstring>

void MatrixClockPlugin::setup()
{
  Screen.clear();
  drops.clear();

  for (int i = 0; i < COLS; i++)
  {
    resetColumn(i, true);
  }

  scene = SCENE_TIME;
  startScene(SCENE_TIME);
}

void MatrixClockPlugin::resetColumn(int index, bool startAbove)
{
  columns[index].y = startAbove ? -static_cast<int8_t>(random(ROWS)) : 0;
  columns[index].previousY = columns[index].y;
  columns[index].speed = random(1, 4);
  columns[index].length = random(3, MAX_TRAIL_LENGTH);
}

void MatrixClockPlugin::buildTarget()
{
  std::memset(target, 0, sizeof(target));

  if (scene == SCENE_WEATHER)
  {
    const WeatherReading reading = weatherStore.get();
    buildWeatherMask(target, reading.temperatureC, reading.icon);
    return;
  }

  if (scene == SCENE_MOON)
  {
    const WeatherReading reading = weatherStore.get();
    buildMoonMask(target, reading.moonIllumination, reading.moonWaxing);
    return;
  }

  struct tm timeinfo;
  if (getLocalTime(&timeinfo))
  {
    buildTimeMask(target, timeinfo.tm_hour, timeinfo.tm_min);
  }
}

void MatrixClockPlugin::startScene(int nextScene)
{
  // Without a reading there is nothing to build, so skip the scenes that need
  // one rather than raining onto an empty target.
  if ((nextScene == SCENE_WEATHER || nextScene == SCENE_MOON) && !weatherStore.hasData())
  {
    nextScene = SCENE_TIME;
  }

  scene = nextScene;
  std::memset(locked, 0, sizeof(locked));
  buildTarget();

  phase = PHASE_RAIN_IN;
  phaseStart = millis();
}

void MatrixClockPlugin::advanceRain()
{
  for (int i = 0; i < COLS; i++)
  {
    columns[i].previousY = columns[i].y;
    columns[i].y += columns[i].speed;

    if (columns[i].y - columns[i].length >= ROWS)
    {
      resetColumn(i, false);
      columns[i].y = -static_cast<int8_t>(random(1, 6));
      columns[i].previousY = columns[i].y;
    }
  }
}

void MatrixClockPlugin::lockCrossedPixels()
{
  for (int x = 0; x < COLS; x++)
  {
    // A fast column skips rows, so sweep everything the head passed over.
    for (int y = columns[x].previousY + 1; y <= columns[x].y; y++)
    {
      if (y < 0 || y >= ROWS)
      {
        continue;
      }
      const int index = y * COLS + x;
      if (target[index] > 0)
      {
        locked[index] = true;
      }
    }
  }
}

bool MatrixClockPlugin::allTargetsLocked() const
{
  for (int i = 0; i < TOTAL_PIXELS; i++)
  {
    if (target[i] > 0 && !locked[i])
    {
      return false;
    }
  }
  return true;
}

void MatrixClockPlugin::lockEverything()
{
  for (int i = 0; i < TOTAL_PIXELS; i++)
  {
    locked[i] = target[i] > 0;
  }
}

void MatrixClockPlugin::scheduleDissolve()
{
  // Bias downward through the image so it comes apart from the top, but jitter
  // each pixel: releasing a whole row at once reads as the row being erased
  // rather than falling.
  for (int y = 0; y < ROWS; y++)
  {
    for (int x = 0; x < COLS; x++)
    {
      const int index = y * COLS + x;
      const unsigned long base = (static_cast<unsigned long>(y) * DISSOLVE_MS) / (ROWS * 2);
      releaseAt[index] = static_cast<uint16_t>(base + random(0, DISSOLVE_MS / 2));
    }
  }
}

void MatrixClockPlugin::releaseLocked(unsigned long elapsed)
{
  for (int y = 0; y < ROWS; y++)
  {
    for (int x = 0; x < COLS; x++)
    {
      const int index = y * COLS + x;
      if (locked[index] && elapsed >= releaseAt[index])
      {
        locked[index] = false;
        drops.push_back({static_cast<int8_t>(x), static_cast<int8_t>(y), target[index]});
      }
    }
  }
}

void MatrixClockPlugin::advanceDrops()
{
  for (size_t i = 0; i < drops.size();)
  {
    drops[i].y++;
    drops[i].brightness =
        drops[i].brightness > DROP_FADE ? drops[i].brightness - DROP_FADE : 0;

    if (drops[i].y >= ROWS || drops[i].brightness == 0)
    {
      drops[i] = drops.back();
      drops.pop_back();
    }
    else
    {
      i++;
    }
  }
}

void MatrixClockPlugin::paint()
{
  Screen.clear();

  // Rain first, dim, so the message reads over it.
  for (int x = 0; x < COLS; x++)
  {
    for (int j = 0; j < columns[x].length; j++)
    {
      const int y = columns[x].y - j;
      if (y < 0 || y >= ROWS)
      {
        continue;
      }
      const uint8_t brightness =
          j == 0 ? RAIN_BRIGHTNESS
                 : static_cast<uint8_t>(RAIN_BRIGHTNESS - (j * RAIN_BRIGHTNESS / columns[x].length));
      if (brightness > 0)
      {
        Screen.setPixel(x, y, 1, brightness);
      }
    }
  }

  // Falling pixels keep the image's brightness so you see it break apart.
  for (const auto &drop : drops)
  {
    Screen.setPixel(drop.x, drop.y, 1, drop.brightness);
  }

  // Locked pixels light at their own target value, which is what carries the
  // shading in the procedural artwork.
  for (int i = 0; i < TOTAL_PIXELS; i++)
  {
    if (locked[i] && target[i] > 0)
    {
      Screen.setPixelAtIndex(i, 1, target[i]);
    }
  }
}

void MatrixClockPlugin::loop()
{
  weatherStore.update();

  if (!timer.isReady(TICK_MS))
  {
    return;
  }

  const unsigned long elapsed = millis() - phaseStart;

  switch (phase)
  {
  case PHASE_RAIN_IN:
    advanceRain();
    lockCrossedPixels();
    // A stream might never cross a given pixel, so the scene always completes
    // on a deadline. Without this the time could stay unreadable indefinitely.
    if (elapsed >= RAIN_IN_MAX_MS || allTargetsLocked())
    {
      lockEverything();
      phase = PHASE_HOLD;
      phaseStart = millis();
    }
    break;

  case PHASE_HOLD:
    advanceRain();
    if (elapsed >= HOLD_MS)
    {
      scheduleDissolve();
      phase = PHASE_DISSOLVE;
      phaseStart = millis();
    }
    break;

  case PHASE_DISSOLVE:
    advanceRain();
    releaseLocked(elapsed);
    advanceDrops();
    if (elapsed >= DISSOLVE_MS)
    {
      startScene((scene + 1) % SCENE_COUNT);
    }
    break;
  }

  paint();
}

const char *MatrixClockPlugin::getName() const
{
  return "Matrix Clock";
}
