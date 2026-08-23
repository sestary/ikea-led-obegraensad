#include "scene_builder.h"
#include "icons.h"
#include "signs.h"

#include <cstring>

namespace
{
// Blank rows between the weather icon and the temperature.
constexpr int WEATHER_GAP = 2;
// Blank columns between temperature glyphs.
constexpr int TEMP_GAP = 1;
// Blank columns between the big time digits. Two rather than one: the big
// digits are 7px wide, so a single column reads as cramped, and the widest pair
// would span 15px and sit on lopsided 0/1 margins. At 2 the widest pair is
// exactly 16px and fills the panel evenly.
constexpr int TIME_GAP = 2;
// The stock minusSymbol is 4px and reads heavy next to the small digits.
constexpr int MINUS_WIDTH = 2;
// Rows the weather artwork is drawn into, above the temperature.
constexpr int ICON_BOX_H = 9;

Glyph minusGlyph()
{
  int bits = 0;
  for (int i = 0; i < MINUS_WIDTH; i++)
  {
    bits |= (0x80 >> i);
  }
  const std::vector<int> data = {0x00, bits, 0x00};
  return captureGlyph(
      [&] { Screen.drawCharacter(0, 0, Screen.readBytes(data), 4, MAX_BRIGHTNESS); });
}

Glyph digitGlyph(int digit)
{
  return captureGlyph([&] { Screen.drawNumbers(0, 0, {digit}); });
}

Glyph bigDigitGlyph(int digit)
{
  return captureGlyph([&] { Screen.drawBigNumbers(0, 0, {digit}); });
}

Glyph degreeGlyph()
{
  return captureGlyph(
      [&] { Screen.drawCharacter(0, 0, Screen.readBytes(degreeSymbol), 4, MAX_BRIGHTNESS); });
}

/** The temperature's glyphs, in reading order. */
std::vector<GlyphItem> temperatureItems(int temperatureC)
{
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
  return items;
}

int tallest(const std::vector<GlyphItem> &items)
{
  int h = 0;
  for (const auto &item : items)
  {
    if (item.glyph.height > h)
    {
      h = item.glyph.height;
    }
  }
  return h;
}

/** Vertical ink extent of an image; false when it is empty. */
bool inkRows(const uint8_t *mask, int &top, int &bottom)
{
  top = ROWS;
  bottom = -1;
  for (int y = 0; y < ROWS; y++)
  {
    for (int x = 0; x < COLS; x++)
    {
      if (mask[y * COLS + x] > 0)
      {
        if (y < top)
        {
          top = y;
        }
        bottom = y;
        break;
      }
    }
  }
  return bottom >= 0;
}
} // namespace

void blitGlyph(uint8_t *mask, const Glyph &glyph, int x, int y)
{
  for (const auto &p : glyph.px)
  {
    const int px = x + p.x;
    const int py = y + p.y;
    if (px >= 0 && px < COLS && py >= 0 && py < ROWS)
    {
      const int index = py * COLS + px;
      if (p.value > mask[index])
      {
        mask[index] = p.value;
      }
    }
  }
}

void composeRow(uint8_t *mask, const std::vector<GlyphItem> &items, int y, int gap)
{
  int total = 0;
  for (size_t i = 0; i < items.size(); i++)
  {
    total += items[i].glyph.width;
    if (i > 0)
    {
      total += gap;
    }
  }
  const int blockHeight = tallest(items);

  int x = (COLS - total) / 2;
  if (x < 0)
  {
    x = 0;
  }

  for (const auto &item : items)
  {
    const int dy = (item.align == GLYPH_TOP) ? 0 : (blockHeight - item.glyph.height) / 2;
    blitGlyph(mask, item.glyph, x, y + dy);
    x += item.glyph.width + gap;
  }
}

void buildTimeMask(uint8_t *mask, int hours, int minutes)
{
  const std::vector<GlyphItem> hh = {{bigDigitGlyph(hours / 10), GLYPH_TOP},
                                     {bigDigitGlyph(hours % 10), GLYPH_TOP}};
  const std::vector<GlyphItem> mm = {{bigDigitGlyph(minutes / 10), GLYPH_TOP},
                                     {bigDigitGlyph(minutes % 10), GLYPH_TOP}};

  composeRow(mask, hh, 0, TIME_GAP);
  composeRow(mask, mm, ROWS - tallest(mm), TIME_GAP);
}

void buildWeatherMask(uint8_t *mask, int temperatureC, int icon)
{
  // Draw once at the origin to find the artwork's real ink extent. The icons do
  // not all fill their box, and the gap below must be measured from the ink
  // rather than the box, or the shorter icons drift away from the temperature.
  uint8_t probe[TOTAL_PIXELS];
  std::memset(probe, 0, sizeof(probe));
  drawWeatherIcon(probe, icon, 0, ICON_BOX_H, MAX_BRIGHTNESS);

  int inkTop = 0, inkBottom = -1;
  const bool hasInk = inkRows(probe, inkTop, inkBottom);
  const int iconHeight = hasInk ? (inkBottom - inkTop + 1) : 0;

  const std::vector<GlyphItem> items = temperatureItems(temperatureC);
  const int tempHeight = tallest(items);

  int top = (ROWS - (iconHeight + WEATHER_GAP + tempHeight)) / 2;
  if (top < 0)
  {
    top = 0;
  }

  int tempY = top + iconHeight + WEATHER_GAP;
  if (tempY + tempHeight > ROWS)
  {
    tempY = ROWS - tempHeight;
  }

  if (hasInk)
  {
    // Shift the box so the artwork's ink starts exactly at `top`.
    drawWeatherIcon(mask, icon, top - inkTop, ICON_BOX_H, MAX_BRIGHTNESS);
  }
  composeRow(mask, items, tempY, TEMP_GAP);
}

void buildMoonMask(uint8_t *mask, double illumination, bool waxing)
{
  drawMoonIcon(mask, illumination, waxing, 0, ROWS, MAX_BRIGHTNESS);
}

void paintMask(const uint8_t *mask, uint8_t scale)
{
  for (int i = 0; i < TOTAL_PIXELS; i++)
  {
    if (mask[i] > 0)
    {
      const int value = (static_cast<int>(mask[i]) * scale) / MAX_BRIGHTNESS;
      if (value > 0)
      {
        Screen.setPixelAtIndex(i, 1, static_cast<uint8_t>(value));
      }
    }
  }
}
