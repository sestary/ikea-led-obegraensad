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

  // 10ms, not the 5 second default: this runs on screenDrawingTask, where
  // blocking waits for NTP freeze the panel and starve async_tcp - the same way
  // the weather fetch used to. Before the first sync there is no time to draw.
  //
  // Not 0 either. getLocalTime loops on `(millis() - start) <= ms`, so a zero
  // timeout returns false without ever reading the clock whenever the
  // millisecond happens to tick over between those two statements - a synced
  // clock that intermittently reports no time at all.
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10))
  {
    buildTimeMask(target, timeinfo.tm_hour, timeinfo.tm_min);
  }
}

int MatrixClockPlugin::chooseNextScene() const
{
  // Two screens that take turns. Without a reading there is no weather to show,
  // so the clock simply stays up.
  if (scene != SCENE_TIME || !weatherStore.hasData())
  {
    return SCENE_TIME;
  }
  return SCENE_WEATHER;
}

bool MatrixClockPlugin::holdComplete(unsigned long elapsed) const
{
  if (elapsed < MIN_HOLD_MS)
  {
    return false;
  }

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10))
  {
    // The weather owns :45 to :00, the time owns the rest, so each scene leaves
    // exactly when the other's window opens.
    const bool weatherWindow = timeinfo.tm_sec >= WEATHER_AT_SECOND;
    return (scene == SCENE_TIME) ? weatherWindow : !weatherWindow;
  }

  // No synced clock, so nothing to pin to - fall back to taking turns.
  return elapsed >= ((scene == SCENE_TIME) ? HOLD_TIME_MS : HOLD_INTERLUDE_MS);
}

bool MatrixClockPlugin::buttonPressed()
{
  int next = (scene + 1) % SCENE_COUNT;
  if (next == SCENE_WEATHER && !weatherStore.hasData())
  {
    next = SCENE_TIME;
  }
  startScene(next);
  return true;
}

void MatrixClockPlugin::startScene(int nextScene)
{
  // Without a reading there is nothing to build, so skip the scenes that need
  // one rather than raining onto an empty target.
  if (nextScene == SCENE_WEATHER && !weatherStore.hasData())
  {
    nextScene = SCENE_TIME;
  }

  scene = nextScene;
  std::memset(locked, 0, sizeof(locked));

  // The dissolve ends on a deadline, so pixels can still be mid-fall when it
  // does. advanceDrops only runs during the dissolve while paint draws drops
  // unconditionally, so any survivor would freeze on the panel and stay lit
  // right through the next scene.
  drops.clear();

  buildTarget();

  phase = PHASE_RAIN_IN;
  phaseStart = millis();
}

void MatrixClockPlugin::advanceRain()
{
  // Draining runs the streams off the bottom for good, so they must not come
  // back round the top the way they do for the rest of a scene.
  const bool recycle = (phase != PHASE_DRAIN);

  for (int i = 0; i < COLS; i++)
  {
    columns[i].previousY = columns[i].y;
    columns[i].y += columns[i].speed;

    if (recycle && columns[i].y - columns[i].length >= ROWS)
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

bool MatrixClockPlugin::rainDrained() const
{
  for (int i = 0; i < COLS; i++)
  {
    // Same test the recycler uses, so a column counts as gone at exactly the
    // point it would otherwise have been sent back to the top.
    if (columns[i].y - columns[i].length < ROWS)
    {
      return false;
    }
  }
  return true;
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

  // Rain first, dim, so the message reads over it. Skipped through the hold:
  // freezing the columns without also hiding them would leave stale trails
  // sitting on top of the scene for its whole duration.
  for (int x = 0; phase != PHASE_HOLD && x < COLS; x++)
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
      phase = PHASE_DRAIN;
      phaseStart = millis();
    }
    break;

  case PHASE_DRAIN:
    advanceRain();
    // A deadline as well as the drain test: a stream that is somehow never
    // finished must not hold the scene open indefinitely.
    if (rainDrained() || elapsed >= DRAIN_MAX_MS)
    {
      phase = PHASE_HOLD;
      phaseStart = millis();
    }
    break;

  case PHASE_HOLD:
    // No advanceRain here: the panel holds the scene clean, and the rain
    // carries only the transitions in and out of it.
    if (holdComplete(elapsed))
    {
      // Bring the columns back in from above. Resuming them where they froze
      // when the hold began would pop half-drawn trails into existence in the
      // middle of the panel.
      for (int i = 0; i < COLS; i++)
      {
        resetColumn(i, true);
      }

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
      startScene(chooseNextScene());
    }
    break;
  }

  paint();
}

const char *MatrixClockPlugin::getName() const
{
  return "Matrix Clock";
}
