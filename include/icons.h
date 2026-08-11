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

/**
 * Vertical LED pitch divided by horizontal, in logical coordinates.
 *
 * The panel is roughly 30 x 50 cm carrying a 16 x 16 grid, so the LEDs are
 * 18.75 mm apart across and 31.25 mm apart down: a pixel is about 1.67x taller
 * than it is wide. A disc drawn with equal pixel radii is therefore an ellipse
 * on the wall, and has to be drawn wider than tall to come out round.
 *
 * Rotation swaps which logical axis maps to which physical one, so the ratio
 * inverts at 90 and 270 degrees - see effectivePixelAspect().
 *
 * This is a measurement of a physical object, not a preference. If a full moon
 * still looks oval on the lamp, this is the number to adjust, and inverting it
 * is the likely fix: which panel edge the buffer's columns run along could not
 * be confirmed from the documentation.
 */
constexpr double PIXEL_ASPECT = 500.0 / 300.0;

/** PIXEL_ASPECT for the current rotation, inverted on the quarter turns. */
double effectivePixelAspect();

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
