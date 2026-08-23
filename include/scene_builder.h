#pragma once

#include "constants.h"
#include "screen.h"
#include <vector>

/**
 * Builds 16x16 target images using the firmware's own drawing helpers.
 *
 * Images are 8-bit, not 1-bit. The panel drives 64 grey levels by temporal
 * PWM, and at this size that shading is what makes a circle read as a circle.
 * Text and the stock bitmaps come out as 0 or 255 either way; the procedural
 * artwork in icons.h is what needs the range.
 *
 * Glyphs are captured individually and unioned rather than drawn one after
 * another, because drawCharacter() writes its blank columns as zeros: two
 * glyphs that overlap by even one column erase each other. Capturing also
 * means layout is driven by each glyph's real ink extent, so nothing depends
 * on the helpers' internal advance widths.
 */

struct GlyphPixel
{
  int8_t x;
  int8_t y;
  uint8_t value;
};

/** A glyph's lit pixels, relative to its own ink bounding box. */
struct Glyph
{
  std::vector<GlyphPixel> px;
  int width = 0;
  int height = 0;
  // Where the ink sat in the drawn image. Needed when a caller must preserve
  // the helper's own grid rather than re-centring on the ink.
  int left = 0;
  int top = 0;
};

/**
 * Draw with the firmware's helpers and capture the lit pixels.
 * Clears the screen, so callers must repaint afterwards.
 */
template <class DrawFn>
Glyph captureGlyph(DrawFn draw)
{
  Screen.clear();
  draw();

  int left = COLS, right = -1, top = ROWS, bottom = -1;
  for (int y = 0; y < ROWS; y++)
  {
    for (int x = 0; x < COLS; x++)
    {
      if (Screen.getBufferIndex(y * COLS + x) > 0)
      {
        if (x < left)
          left = x;
        if (x > right)
          right = x;
        if (y < top)
          top = y;
        if (y > bottom)
          bottom = y;
      }
    }
  }

  Glyph glyph;
  if (right < 0)
  {
    Screen.clear();
    return glyph;
  }

  glyph.width = right - left + 1;
  glyph.height = bottom - top + 1;
  glyph.left = left;
  glyph.top = top;
  for (int y = top; y <= bottom; y++)
  {
    for (int x = left; x <= right; x++)
    {
      const uint8_t v = Screen.getBufferIndex(y * COLS + x);
      if (v > 0)
      {
        glyph.px.push_back({static_cast<int8_t>(x - left), static_cast<int8_t>(y - top), v});
      }
    }
  }

  Screen.clear();
  return glyph;
}

enum GlyphAlign
{
  GLYPH_TOP,
  GLYPH_MIDDLE
};

struct GlyphItem
{
  Glyph glyph;
  GlyphAlign align;
};

/** Union a glyph into mask at the given top-left. Out-of-bounds is clipped. */
void blitGlyph(uint8_t *mask, const Glyph &glyph, int x, int y);

/** Union a row of glyphs into mask, centred horizontally, `gap` px apart. */
void composeRow(uint8_t *mask, const std::vector<GlyphItem> &items, int y, int gap);

/**
 * Hours across the top, minutes flush to the bottom edge.
 *
 * Digits are packed proportionally and each row centred, matching the
 * temperature. Only the digit 1 is narrow - 4px against 7px for the rest - so
 * the row width changes with the digits and the time shifts slightly as the
 * minutes tick. That is the accepted trade for the tighter setting.
 */
void buildTimeMask(uint8_t *mask, int hours, int minutes);

/**
 * Weather icon above a centred temperature, vertically centred as a block with
 * a two-row gap. The icon is drawn procedurally by icons.h rather than taken
 * from the stock bitmaps, so it carries shading.
 */
void buildWeatherMask(uint8_t *mask, int temperatureC, int icon);

/** The moon, filling the panel. */
void buildMoonMask(uint8_t *mask, double illumination, bool waxing);

/** Paint an image onto the screen. `scale` dims the whole thing: 255 is full. */
void paintMask(const uint8_t *mask, uint8_t scale);
