# Matrix Clock — Design

**Date:** 2026-08-10
**Status:** Implemented on branch `tui-simulator`

## Idea

A new effect where Matrix-style falling blocks condense into the time, dissolve
back into rain, condense into the weather, and repeat.

Each lit block *is* a letter, in the same visual vocabulary as the existing
`MatrixRainPlugin`. No glyph font is involved in the rain: at 16×16 a 5×7 font
would fit only three columns across, which reads as sparse rather than dense.

## Scenes

Three scenes, cycled forever, with a rain transition between each:

| # | Scene   | Built with                                                            |
|---|---------|-----------------------------------------------------------------------|
| 1 | Time    | `drawBigNumbers` — HH across the top, MM flush to the bottom edge     |
| 2 | Weather | a procedural icon above a centred temperature                         |
| 3 | Moon    | the current phase, filling the panel                                  |

The weather and moon scenes are skipped until a reading arrives, so the cycle
runs time-only rather than raining onto an empty target.

An earlier draft made the icon and the temperature separate scenes, on the
reasoning that each would get the full panel. Rendering them proved that wrong:
the icons are 5–8 rows tall and the temperature uses 5-row digits, so alone
each leaves most of the panel dark. That is why `WeatherPlugin` already
composes them onto one screen.

Minutes sit flush to the bottom row rather than at `ROWS/2`, which fills the
panel edge to edge and opens the gap between the two rows from one to two —
matching the weather screen's spacing.

Horizontally the digits are packed proportionally and each row centred,
matching how the temperature is set. The gap scales with the glyph: two blank
columns between the 7px time digits, one between the 3px temperature digits.

A single column between the big digits reads as cramped, and makes the widest
pair span 15px, which centres onto lopsided 0/1 margins. At two columns the
widest pair is exactly 16px and fills the panel evenly.

Only the digit `1` is narrow: 4px against 7px for every other digit, on
`drawBigNumbers`' 8px cell pitch. On that fixed grid the `1` sits hard against
the right of its cell, which leaves `11` looking gappy.

The trade-off is accepted deliberately: because the row width changes with the
digits (9px for `11`, 15px for `23`), the time shifts sideways as the minutes
tick, and the two rows no longer share a column grid. A fixed grid would keep
the time perfectly still — the usual reason clocks use tabular figures — but
was judged to look worse here.

## Targets come from the firmware's own drawing code

A scene's target is a `bool[TOTAL_PIXELS]` mask. Each glyph is captured by:

1. `Screen.clear()`
2. call the existing helper (`drawBigNumbers`, `drawWeather`, …)
3. read the buffer back via `Screen.getBufferIndex(i)`, recording `> 0`
4. `Screen.clear()`

No glyph data is duplicated. The digits and icons are pixel-identical to Big
Clock and Weather, and stay identical if those fonts ever change. This is the
central design decision; everything else is animation on top of it.

### Images are 8-bit

The panel drives 64 grey levels by temporal PWM (`GRAY_LEVELS = 64`, a render
pass every 200 µs on ESP32 giving ~78 Hz). 1-bit artwork throws that away, and
at 16×16 the shading is what makes a circle read as a circle.

Every image is therefore `uint8_t[TOTAL_PIXELS]`, and a locked pixel lights at
its own value rather than a flat full brightness. Text and the stock bitmaps
come out 0 or 255 either way; only the procedural artwork uses the range.

One caveat: grey resolution scales with global brightness, since
`scaledValue = value × brightness / 255`. At half brightness only 32 steps
remain, so the moon's terminator bands on a dimmed lamp.

### The artwork is procedural

`icons.cpp` rasterises the weather icons and the moon from primitives — disc,
box, capsule, sun, cloud, bolt — supersampled 6×6. Shapes are defined once in a
16×9 reference box and scaled to whatever box the caller asks for.

Two details that are not obvious and were each found by looking at the output:

- **The sun's rays start at the disc's edge.** A gap between disc and rays
  leaves a dark ring, which at this size reads as an eye rather than a sun.
- **The moon's disc leaves a margin.** An inscribed disc spans all 16 columns
  and reads as a blob filling the panel. Its unlit face is also drawn faintly,
  so a new moon is a dark disc rather than a blank panel.

### Glyphs are unioned, never drawn over each other

`Screen_::drawCharacter` writes its blank columns as **zeros**, so two glyphs
overlapping by even one column erase each other. This silently clipped the
minus sign to 3px, and clipped the digits too once spacing tightened.

Every composed image therefore captures each glyph separately and ORs the lit
pixels into the mask. Overlap becomes harmless, and layout is driven by each
glyph's measured ink extent rather than by the helpers' internal advance widths
(`drawNumbers` insets its ink by 1px, `drawCharacter` insets the degree by 2px
— modelling those was the source of repeated off-by-one errors).

This lives in `scene_builder.h` / `scene_builder.cpp` as `captureGlyph`,
`blitGlyph`, `composeRow`, `buildTimeMask`, `buildWeatherMask` and `paintMask`.

### Weather screen layout

Icon heights vary from 5 rows (clear, cloudy) to 8 (thunder, rain, snow), so a
fixed position per condition cannot hold a consistent gap — the existing
plugin leaves the tall icons only one blank row. The icon is measured, the
temperature placed two rows below it, and the whole block centred vertically.
The temperature is centred horizontally from its measured width. The minus sign
is a deliberate 2px, narrower than the stock 4px `minusSymbol`, so `-12°` still
clears both edges.

## Phases

Each scene runs `RAIN_IN → HOLD → DISSOLVE`, then advances.

**RAIN_IN.** Columns fall as in `MatrixRainPlugin` — a bright head with a fading
trail — but drawn dim (brightness 90) so they read as background noise. When a
column head reaches a row where the target mask is set, that pixel locks at full
brightness (255) and stays lit for the rest of the scene.

Because a stream might never cross a given pixel, RAIN_IN has a deadline: any
pixels still unlocked at `RAIN_IN_MAX_MS` lock at once. The scene always
completes, and the time is always legible. This is a correctness requirement,
not a polish detail.

**HOLD.** The locked mask is displayed. Rain continues behind it at low
brightness so the panel never looks frozen.

**DISSOLVE.** Each locked pixel is given a release time biased down the image
but jittered per pixel, so the picture crumbles rather than peeling off a row
at a time — releasing whole rows together reads as the row being erased.

A released pixel becomes a drop that keeps the image's brightness and fades as
it falls, so you see the picture come apart. Painting drops at rain brightness
makes them indistinguishable from the background and the image just seems to
vanish. Drops survive the scene change and finish falling behind the next
rain-in.

The plugin repaints from its own state every tick rather than reading the screen
buffer back and decaying it. Read-back happens once per scene, to build the
mask. This keeps the animation deterministic and independent of what else has
touched the buffer.

## Orientation

The effect works with the lamp hung landscape or portrait, and needs no
orientation-specific code.

Plugins draw in logical space; `currentRotation` is applied downstream in
`Screen_::_render()` via `getRotatedRenderBuffer()`, as compensation for how the
panel is physically mounted. Logical "down" is therefore the viewer's "down" at
every rotation, so the rain falls downward in all four orientations. Tracing
rotation 1, a drop moving down in logical space becomes a decreasing physical
column, which is downward to a viewer looking at a panel hung rotated.

The panel is square, so there is no aspect change between orientations either:
the HH/MM stacking and the 16×16 icon lay out identically.

The plugin must consequently never read `Screen.currentRotation` or apply any
rotation of its own. Doing so would rotate the image twice.

## Timing

| Constant          | Value  |
|-------------------|--------|
| tick              | 50 ms  |
| `RAIN_IN_MAX_MS`  | 2500   |
| `HOLD_MS`         | 8000   |
| `DISSOLVE_MS`     | 1200   |

One full cycle — three scenes — is about 35 seconds.

## Shared weather store

Matrix Clock needs the temperature and icon that `WeatherPlugin` already fetches
and caches privately. Rather than a second wttr.in request, the fetch and cache
move into a small shared store:

```cpp
struct WeatherReading {
  int temperatureC;
  int icon;   // index into weatherIcons
  bool valid;
};
```

`WeatherStore` exposes `update()` (rate-limited HTTP fetch and parse),
`get()`, and `hasData()`. `WeatherPlugin` is reduced to a renderer that reads
the store, and now shares `buildWeatherMask`, so it gains the corrected
spacing, centring and minus. It keeps its existing brightness of 100; Matrix
Clock paints its locked pixels at full brightness.

wttr.in needs no API key — the request is
`https://wttr.in/{location}?format=j2&lang=en`. Sharing is about one update
schedule and one copy of the parsing code, not about API cost.

In the simulator this comes free: the store goes through the same stubbed
`HTTPClient`, so both plugins see the canned forecast and `w` cycles conditions
for both.

If the store has no reading yet, the weather scene is skipped and the cycle
runs time-only until data arrives.

## Files

- Create `include/icons.h`, `src/icons.cpp`
- Create `include/scene_builder.h`, `src/scene_builder.cpp`
- Create `include/weather_store.h`, `src/weather_store.cpp`
- Create `include/plugins/MatrixClockPlugin.h`, `src/plugins/MatrixClockPlugin.cpp`
- Modify `src/plugins/WeatherPlugin.cpp` and its header — move fetch/cache to the store
- Modify `src/main.cpp` — register the plugin
- Modify `sim/host/sim_registry.cpp` — register it for the simulator (28 → 29)

## Testing

The simulator makes this testable rather than eyeball-only. Tests drive the
simulated clock, so they are deterministic despite the rain using `random()`.

- After `RAIN_IN_MAX_MS`, the frame equals the target mask exactly — the
  deadline guarantee, and the most important test.
- A scene's mask matches drawing the same content directly through `Screen`,
  proving the snapshot approach agrees with Big Clock and Weather.
- The cycle reaches the weather scene and wraps back to the time.
- DISSOLVE eventually clears every locked pixel.
- With no weather reading, the cycle runs time-only and never stalls.
- A full cycle runs without crashing or writing out of bounds.
- The locked frame at each rotation equals the rotation of the frame at
  rotation 0, confirming the plugin adds no rotation of its own.
- During the dissolve, pixels brighter than the rain appear at positions the
  target never lit — proving released pixels travel rather than disappearing.
- During the dissolve, some row holds part of its target pixels while the rest
  have let go, proving it crumbles rather than erasing rows.

## Out of scope

- 12-hour format, seconds, or a date scene.
- Configuring timings or scene order from the web UI.
- Changing `MatrixRainPlugin`; the rain code is re-implemented inside the new
  plugin because its needs differ (dim rendering, locking, dissolve). Extracting
  a shared rain engine for two plugins with different requirements would couple
  them for little gain.
