#pragma once

#include "constants.h"
#include "screen.h"
#include <vector>

/**
 * Builds 16x16 target masks using the firmware's own drawing helpers.
 *
 * Glyphs are captured individually and unioned rather than drawn one after
 * another, because drawCharacter() writes its blank columns as zeros: two
 * glyphs that overlap by even one column erase each other. Capturing also
 * means layout is driven by each glyph's real ink extent, so nothing depends
 * on the helpers' internal advance widths.
 */

/** A glyph's lit pixels, relative to its own ink bounding box. */
struct Glyph
{
  std::vector<std::pair<int8_t, int8_t>> px;
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
      if (Screen.getBufferIndex(y * COLS + x) > 0)
      {
        glyph.px.push_back({static_cast<int8_t>(x - left), static_cast<int8_t>(y - top)});
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
void blitGlyph(bool *mask, const Glyph &glyph, int x, int y);

/** Union a row of glyphs into mask, centred horizontally, `gap` px apart. */
void composeRow(bool *mask, const std::vector<GlyphItem> &items, int y, int gap);

/**
 * Hours across the top, minutes flush to the bottom edge.
 *
 * Both rows keep drawBigNumbers' own column grid: centring each row on its
 * measured ink pulls them out of line, because digit widths differ.
 */
void buildTimeMask(bool *mask, int hours, int minutes);

/**
 * Weather icon above a centred temperature, vertically centred as a block with
 * a fixed gap. Icons vary from 5 to 8 rows tall, so the gap is measured rather
 * than tabulated.
 */
void buildWeatherMask(bool *mask, int temperatureC, int icon);

/** Paint a mask onto the screen at one brightness. */
void paintMask(const bool *mask, uint8_t brightness);
