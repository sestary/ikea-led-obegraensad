#include "icons.h"
#include "screen.h"

#include <cmath>

namespace
{
// Subsamples per axis. 6x6 is enough to make the limb of a disc smooth at this
// size, and keeps the per-scene cost small - icons are built once per scene,
// not per frame.
constexpr int SUBSAMPLES = 6;

constexpr double PI_D = 3.14159265358979323846;

struct Pt
{
  double x, y;
};

bool disc(Pt p, double cx, double cy, double r)
{
  const double dx = p.x - cx, dy = p.y - cy;
  return dx * dx + dy * dy <= r * r;
}

bool box(Pt p, double x0, double y0, double x1, double y1)
{
  return p.x >= x0 && p.x <= x1 && p.y >= y0 && p.y <= y1;
}

/** Rounded bar, used for fog banks and rain streaks. */
bool capsule(Pt p, double x0, double y0, double x1, double y1, double r)
{
  const double dx = x1 - x0, dy = y1 - y0;
  const double len2 = dx * dx + dy * dy;
  double t = len2 > 0 ? ((p.x - x0) * dx + (p.y - y0) * dy) / len2 : 0.0;
  t = t < 0 ? 0 : (t > 1 ? 1 : t);
  const double px = x0 + t * dx - p.x;
  const double py = y0 + t * dy - p.y;
  return px * px + py * py <= r * r;
}

/**
 * Sun: a solid disc inside a corona that falls away around it.
 *
 * Rays were tried in four arrangements - attached, detached, four cardinal and
 * twelve fine - and every one turns to mush at this size. The corona says
 * "this glows" through the panel's grey levels instead of through shape, which
 * is what the hardware is actually good at.
 *
 * It carries no rays at all, so it leans entirely on the falloff to read as a
 * sun. That is what keeps it apart from the moon, whose disc is crisp-edged
 * and carries maria.
 *
 * Returns 0..255 rather than a boolean, since the falloff is the whole point.
 */
int sunGlow(Pt p, double cx, double cy, double core, double coronaOut, int coronaPeak)
{
  const double dx = p.x - cx, dy = p.y - cy;
  const double d = std::sqrt(dx * dx + dy * dy);

  if (d <= core)
  {
    return 255;
  }
  if (d >= coronaOut)
  {
    return 0;
  }

  // Squared falloff: linear leaves a visible edge where the corona stops.
  const double t = (d - core) / (coronaOut - core);
  const double falloff = (1.0 - t) * (1.0 - t);
  return static_cast<int>(coronaPeak * falloff);
}

/** Cloud: three lobes over a flat base. */
bool cloud(Pt p, double cx, double cy, double s)
{
  return disc(p, cx - 2.5 * s, cy + 0.35 * s, 1.75 * s) ||
         disc(p, cx + 0.1 * s, cy - 0.75 * s, 2.35 * s) ||
         disc(p, cx + 2.7 * s, cy + 0.30 * s, 1.85 * s) ||
         box(p, cx - 4.2 * s, cy + 0.30 * s, cx + 4.5 * s, cy + 2.05 * s);
}

bool bolt(Pt p, double cx, double cy)
{
  return capsule(p, cx + 1.5, cy - 0.4, cx - 0.8, cy + 2.4, 0.85) ||
         capsule(p, cx - 0.8, cy + 2.2, cx + 1.2, cy + 2.2, 0.80) ||
         capsule(p, cx + 1.2, cy + 2.0, cx - 1.2, cy + 5.0, 0.85);
}

/** Shape membership in reference-box coordinates: 16 wide, 15 tall, square. */
bool insideIcon(int icon, Pt p)
{
  switch (icon)
  {
  case 2: // clear - handled by iconValue, which needs the falloff
    return false;

  case 0: // cloudy
    return cloud(p, 8.0, 6.6, 1.80);

  case 3: // partly cloudy - the sun is added by iconValue
    return cloud(p, 7.0, 9.0, 1.35);

  case 4: // rain
    return cloud(p, 8.0, 4.9, 1.50) || capsule(p, 5.4, 10.0, 4.4, 13.8, 0.70) ||
           capsule(p, 8.2, 10.0, 7.2, 13.8, 0.70) ||
           capsule(p, 11.0, 10.0, 10.0, 13.8, 0.70);

  case 5: // snow
    return cloud(p, 8.0, 4.9, 1.50) || disc(p, 5.0, 11.0, 1.05) ||
           disc(p, 8.0, 13.2, 1.05) || disc(p, 11.0, 11.0, 1.05);

  case 1: // thunder
    return cloud(p, 8.0, 4.6, 1.45) || bolt(p, 8.0, 8.8);

  case 6: // fog
    return capsule(p, 3.4, 3.4, 12.2, 3.4, 1.15) ||
           capsule(p, 4.8, 7.5, 13.2, 7.5, 1.15) ||
           capsule(p, 3.0, 11.6, 11.6, 11.6, 1.15);

  default:
    return cloud(p, 8.0, 6.6, 1.80);
  }
}

/** An icon's value at a point, 0..255. Most shapes are solid; the sun glows. */
int iconValue(int icon, Pt p)
{
  if (icon == 2)
  {
    return sunGlow(p, 8.0, 7.5, 4.60, 7.40, 150);
  }
  if (icon == 3)
  {
    const int glow = sunGlow(p, 11.2, 4.6, 2.30, 4.40, 140);
    return insideIcon(icon, p) ? 255 : glow;
  }
  return insideIcon(icon, p) ? 255 : 0;
}

void writeMax(uint8_t *mask, int index, int value)
{
  if (value > mask[index])
  {
    mask[index] = static_cast<uint8_t>(value > 255 ? 255 : value);
  }
}
} // namespace

double effectivePixelAspect()
{
  // A quarter turn maps logical x onto the panel axis logical y used to sit on,
  // so the two pitches trade places.
  return (Screen.currentRotation & 1) ? (1.0 / PIXEL_ASPECT) : PIXEL_ASPECT;
}

void drawWeatherIcon(uint8_t *mask, int icon, int top, int height, uint8_t peak)
{
  if (height <= 0)
  {
    return;
  }

  // Work in physical units: one unit is a horizontal LED pitch, so a row is
  // PIXEL_ASPECT units tall. A uniform scale in that space keeps circles round
  // on the wall, which is not the same as round in the buffer.
  const double aspect = effectivePixelAspect();
  const double boxH = height * aspect;
  const double scaleX = COLS / static_cast<double>(ICON_REF_W);
  const double scaleY = boxH / static_cast<double>(ICON_REF_H);
  const double scale = (scaleX < scaleY) ? scaleX : scaleY;
  const double offsetX = (COLS - ICON_REF_W * scale) / 2.0;
  const double offsetY = (boxH - ICON_REF_H * scale) / 2.0;

  for (int y = 0; y < height; y++)
  {
    const int row = top + y;
    if (row < 0 || row >= ROWS)
    {
      continue;
    }
    for (int x = 0; x < COLS; x++)
    {
      int accumulated = 0;
      for (int sy = 0; sy < SUBSAMPLES; sy++)
      {
        for (int sx = 0; sx < SUBSAMPLES; sx++)
        {
          const double px = x + (sx + 0.5) / SUBSAMPLES - 0.5;
          const double py = (y + (sy + 0.5) / SUBSAMPLES - 0.5) * aspect;
          const Pt p = {(px - offsetX) / scale, (py - offsetY) / scale};
          if (p.x >= -1.0 && p.x <= ICON_REF_W + 1.0)
          {
            accumulated += iconValue(icon, p);
          }
        }
      }
      const int value = accumulated / (SUBSAMPLES * SUBSAMPLES);
      if (value > 0)
      {
        writeMax(mask, row * COLS + x, (value * peak) / MAX_BRIGHTNESS);
      }
    }
  }
}

void drawMoonIcon(uint8_t *mask, double illumination, bool waxing, int top, int height,
                  uint8_t peak)
{
  if (height <= 0)
  {
    return;
  }

  // The unlit face, faint enough to read as shadow rather than light.
  const int dim = (peak * 38) / 255;

  // Maria. Without them a full moon is a plain bright disc, which is hard to
  // tell from the clear-weather sun.
  struct Mare
  {
    double x, y, r;
  };
  static const Mare maria[] = {
      {-0.34, -0.30, 0.30}, {0.20, -0.42, 0.22}, {0.32, 0.26, 0.26}, {-0.24, 0.40, 0.18}};

  // Round on the wall, not in the buffer: the horizontal radius is the vertical
  // one scaled by the pixel aspect, and both are capped to fit with a margin.
  const double aspect = effectivePixelAspect();
  const double maxRx = (COLS / 2.0) - 1.0;
  const double maxRy = (height / 2.0) - 1.0;
  const double ry = (maxRx / aspect < maxRy) ? (maxRx / aspect) : maxRy;
  const double rx = ry * aspect;

  const double cx = (COLS - 1) / 2.0;
  const double cy = top + (height - 1) / 2.0;
  const double c = 1.0 - 2.0 * illumination;

  for (int y = 0; y < height; y++)
  {
    const int row = top + y;
    if (row < 0 || row >= ROWS)
    {
      continue;
    }
    for (int x = 0; x < COLS; x++)
    {
      int inDisc = 0, inLit = 0, inMare = 0;
      for (int sy = 0; sy < SUBSAMPLES; sy++)
      {
        for (int sx = 0; sx < SUBSAMPLES; sx++)
        {
          const double px = x + (sx + 0.5) / SUBSAMPLES - 0.5;
          const double py = row + (sy + 0.5) / SUBSAMPLES - 0.5;
          const double nx = (px - cx) / rx;
          const double ny = (py - cy) / ry;
          if (nx * nx + ny * ny > 1.0)
          {
            continue;
          }
          inDisc++;

          const double R = std::sqrt(1.0 - ny * ny);
          if (waxing ? (nx >= c * R) : (nx <= -c * R))
          {
            inLit++;
            for (const Mare &m : maria)
            {
              const double dx = nx - m.x, dy = ny - m.y;
              if (dx * dx + dy * dy < m.r * m.r)
              {
                inMare++;
                break;
              }
            }
          }
        }
      }
      if (inDisc == 0)
      {
        continue;
      }

      const int total = SUBSAMPLES * SUBSAMPLES;
      const int unlit = inDisc - inLit;
      double v = (unlit * dim + inLit * peak) / static_cast<double>(total);
      v -= (static_cast<double>(inMare) / total) * (peak - dim) * 0.42;
      if (v < 0)
      {
        v = 0;
      }
      writeMax(mask, row * COLS + x, static_cast<int>(v));
    }
  }
}
