#include "scene_builder.h"
#include "signs.h"

namespace
{
// Blank rows between the weather icon and the temperature.
constexpr int WEATHER_GAP = 2;
// Blank columns between temperature glyphs.
constexpr int TEMP_GAP = 1;
// The stock minusSymbol is 4px and reads heavy next to the small digits.
constexpr int MINUS_WIDTH = 2;

Glyph minusGlyph()
{
  int bits = 0;
  for (int i = 0; i < MINUS_WIDTH; i++)
  {
    bits |= (0x80 >> i);
  }
  const std::vector<int> data = {0x00, bits, 0x00};
  return captureGlyph([&] { Screen.drawCharacter(0, 0, Screen.readBytes(data), 4, MAX_BRIGHTNESS); });
}

Glyph digitGlyph(int digit)
{
  return captureGlyph([&] { Screen.drawNumbers(0, 0, {digit}); });
}

Glyph degreeGlyph()
{
  return captureGlyph(
      [&] { Screen.drawCharacter(0, 0, Screen.readBytes(degreeSymbol), 4, MAX_BRIGHTNESS); });
}
} // namespace

void blitGlyph(bool *mask, const Glyph &glyph, int x, int y)
{
  for (const auto &p : glyph.px)
  {
    const int px = x + p.first;
    const int py = y + p.second;
    if (px >= 0 && px < COLS && py >= 0 && py < ROWS)
    {
      mask[py * COLS + px] = true;
    }
  }
}

void composeRow(bool *mask, const std::vector<GlyphItem> &items, int y, int gap)
{
  int total = 0;
  int tallest = 0;
  for (size_t i = 0; i < items.size(); i++)
  {
    total += items[i].glyph.width;
    if (i > 0)
    {
      total += gap;
    }
    if (items[i].glyph.height > tallest)
    {
      tallest = items[i].glyph.height;
    }
  }

  int x = (COLS - total) / 2;
  if (x < 0)
  {
    x = 0;
  }

  for (const auto &item : items)
  {
    const int dy = (item.align == GLYPH_TOP) ? 0 : (tallest - item.glyph.height) / 2;
    blitGlyph(mask, item.glyph, x, y + dy);
    x += item.glyph.width + gap;
  }
}

void buildTimeMask(bool *mask, int hours, int minutes)
{
  const Glyph hh =
      captureGlyph([&] { Screen.drawBigNumbers(0, 0, {hours / 10, hours % 10}); });
  const Glyph mm =
      captureGlyph([&] { Screen.drawBigNumbers(0, 0, {minutes / 10, minutes % 10}); });

  blitGlyph(mask, hh, (COLS - hh.width) / 2, 0);
  blitGlyph(mask, mm, (COLS - mm.width) / 2, ROWS - mm.height);
}

void buildWeatherMask(bool *mask, int temperatureC, int icon)
{
  const Glyph iconGlyph =
      captureGlyph([&] { Screen.drawWeather(0, 0, icon, MAX_BRIGHTNESS); });

  const bool negative = temperatureC < 0;
  const int value = negative ? -temperatureC : temperatureC;

  std::vector<GlyphItem> items;
  if (negative)
  {
    items.push_back({minusGlyph(), GLYPH_MIDDLE});
  }
  if (value >= 10)
  {
    items.push_back({digitGlyph(value / 10), GLYPH_TOP});
  }
  items.push_back({digitGlyph(value % 10), GLYPH_TOP});
  items.push_back({degreeGlyph(), GLYPH_TOP});

  int tempHeight = 0;
  for (const auto &item : items)
  {
    if (item.glyph.height > tempHeight)
    {
      tempHeight = item.glyph.height;
    }
  }

  int top = (ROWS - (iconGlyph.height + WEATHER_GAP + tempHeight)) / 2;
  if (top < 0)
  {
    top = 0;
  }

  int tempY = top + iconGlyph.height + WEATHER_GAP;
  if (tempY + tempHeight > ROWS)
  {
    tempY = ROWS - tempHeight;
  }

  blitGlyph(mask, iconGlyph, (COLS - iconGlyph.width) / 2, top);
  composeRow(mask, items, tempY, TEMP_GAP);
}

void paintMask(const bool *mask, uint8_t brightness)
{
  for (int i = 0; i < TOTAL_PIXELS; i++)
  {
    if (mask[i])
    {
      Screen.setPixelAtIndex(i, 1, brightness);
    }
  }
}
