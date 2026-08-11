#pragma once

#include "constants.h"
#include <cstdint>

/**
 * Anti-aliased procedural artwork.
 *
 * The panel drives 64 grey levels by temporal PWM, which 1-bit bitmaps throw
 * away. At 16x16 that shading is what makes a circle read as a circle rather
 * than a stepped blob, so the weather icons and the moon are rasterised here
 * instead of being stored as bitmaps.
 *
 * Every shape is supersampled; a pixel's value is the fraction of it the shape
 * covers, scaled by `peak`. Shapes are defined in a reference box 16 wide and
 * 9 tall and scaled to whatever box the caller asks for, so the same artwork
 * serves both the small icon above the temperature and a full-panel version.
 */

/** Reference box the artwork is designed in. */
constexpr int ICON_REF_W = 16;
constexpr int ICON_REF_H = 9;

/**
 * Rasterise weather icon `icon` (an index into the same set weatherIcons uses)
 * into `mask`, fitted to the box spanning `height` rows from row `top`.
 * Values are unioned with whatever is already in the mask, so nothing is erased.
 */
void drawWeatherIcon(uint8_t *mask, int icon, int top, int height, uint8_t peak);

/**
 * Rasterise the moon at the given illumination (0..1). Waxing is lit from the
 * right, waning from the left. The unlit face is drawn faintly so the whole
 * disc stays visible - without it a new moon is a blank panel.
 */
void drawMoonIcon(uint8_t *mask, double illumination, bool waxing, int top, int height,
                  uint8_t peak);
