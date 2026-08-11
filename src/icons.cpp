#include "icons.h"

#include <cmath>

namespace
{
// Subsamples per axis. 6x6 is enough to make the limb of a disc smooth at this
// size, and keeps the per-scene cost small - icons are built once per scene,
// not per frame.
constexpr int SS = 6;

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
 * Sun: a disc with eight tapered rays.
 *
 * The rays start at the disc's edge rather than beyond it. A gap between the
 * two leaves a dark ring at this size, which reads as an eye or a flower
 * rather than a sun.
 */
bool sun(Pt p, double cx, double cy, double core, double rayIn, double rayOut)
{
  if (disc(p, cx, cy, core))
    return true;

  const double dx = p.x - cx, dy = p.y - cy;
  const double d = std::sqrt(dx * dx + dy * dy);
  if (d < rayIn || d > rayOut)
    return false;

  double a = std::atan2(dy, dx);
  if (a < 0)
    a += 2 * PI_D;
  const double seg = 2 * PI_D / 8;
  const double off = std::fmod(a + seg / 2, seg) - seg / 2;
  const double t = (d - rayIn) / (rayOut - rayIn);
  return std::fabs(off) <= 0.30 * (1.0 - 0.60 * t);
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
  return capsule(p, cx + 0.9, cy - 0.2, cx - 0.5, cy + 1.5, 0.55) ||
         capsule(p, cx - 0.5, cy + 1.4, cx + 0.7, cy + 1.4, 0.5) ||
         capsule(p, cx + 0.7, cy + 1.3, cx - 0.7, cy + 3.1, 0.55);
}

/** Shape membership in reference-box coordinates: 16 wide, 9 tall. */
bool insideIcon(int icon, Pt p)
{
  switch (icon)
  {
  case 2: // clear
    return sun(p, 8.0, 4.5, 2.10, 2.10, 4.45);

  case 0: // cloudy
    return cloud(p, 8.0, 4.2, 1.0);

  case 3: // partly cloudy
    return sun(p, 11.5, 2.8, 1.45, 1.45, 3.05) || cloud(p, 6.6, 5.2, 0.86);

  case 4: // rain
    return cloud(p, 8.0, 3.1, 0.86) || capsule(p, 5.6, 6.6, 4.9, 8.4, 0.42) ||
           capsule(p, 8.2, 6.6, 7.5, 8.4, 0.42) ||
           capsule(p, 10.8, 6.6, 10.1, 8.4, 0.42);

  case 5: // snow
    return cloud(p, 8.0, 3.1, 0.86) || disc(p, 5.3, 7.2, 0.62) ||
           disc(p, 8.0, 7.9, 0.62) || disc(p, 10.7, 7.2, 0.62);

  case 1: // thunder
    return cloud(p, 8.0, 2.9, 0.84) || bolt(p, 8.0, 5.4);

  case 6: // fog
    return capsule(p, 3.4, 2.2, 12.0, 2.2, 0.72) ||
           capsule(p, 4.6, 4.3, 13.0, 4.3, 0.72) ||
           capsule(p, 3.0, 6.4, 11.4, 6.4, 0.72);

  default:
    return cloud(p, 8.0, 4.2, 1.0);
  }
}

void writeMax(uint8_t *mask, int index, int value)
{
  if (value > mask[index])
  {
    mask[index] = static_cast<uint8_t>(value > 255 ? 255 : value);
  }
}
} // namespace

void drawWeatherIcon(uint8_t *mask, int icon, int top, int height, uint8_t peak)
{
  if (height <= 0)
  {
    return;
  }

  // Uniform scale keeps circles round; the artwork is centred in the box.
  const double scale = static_cast<double>(height) / ICON_REF_H;
  const double drawnW = ICON_REF_W * scale;
  const double offsetX = (COLS - drawnW) / 2.0;

  for (int y = 0; y < height; y++)
  {
    const int row = top + y;
    if (row < 0 || row >= ROWS)
    {
      continue;
    }
    for (int x = 0; x < COLS; x++)
    {
      int hits = 0;
      for (int sy = 0; sy < SS; sy++)
      {
        for (int sx = 0; sx < SS; sx++)
        {
          const double px = x + (sx + 0.5) / SS - 0.5;
          const double py = y + (sy + 0.5) / SS - 0.5;
          const Pt p = {(px - offsetX) / scale, py / scale};
          if (p.x >= 0 && p.x <= ICON_REF_W && insideIcon(icon, p))
          {
            hits++;
          }
        }
      }
      if (hits > 0)
      {
        writeMax(mask, row * COLS + x, (hits * peak) / (SS * SS));
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

  const double r = (height / 2.0) - 1.0; // margin: an inscribed disc reads as a blob
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
      for (int sy = 0; sy < SS; sy++)
      {
        for (int sx = 0; sx < SS; sx++)
        {
          const double px = x + (sx + 0.5) / SS - 0.5;
          const double py = row + (sy + 0.5) / SS - 0.5;
          const double nx = (px - cx) / r;
          const double ny = (py - cy) / r;
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

      const int total = SS * SS;
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
