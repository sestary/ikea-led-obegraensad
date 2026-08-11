# Matrix Clock — Design

**Date:** 2026-08-10
**Status:** Approved, ready for implementation planning

## Idea

A new effect where Matrix-style falling blocks condense into the time, dissolve
back into rain, condense into the weather, and repeat.

Each lit block *is* a letter, in the same visual vocabulary as the existing
`MatrixRainPlugin`. No glyph font is involved in the rain: at 16×16 a 5×7 font
would fit only three columns across, which reads as sparse rather than dense.

## Scenes

Three scenes, cycled forever, with a rain transition between **every** one:

| # | Scene       | Built with                                            |
|---|-------------|-------------------------------------------------------|
| 1 | Time        | `drawBigNumbers` — HH on the top half, MM on the bottom |
| 2 | Weather icon| `drawWeather` — full 16×16                              |
| 3 | Temperature | `drawNumbers` plus `degreeSymbol` / `minusSymbol`      |

Icon and temperature are separate scenes rather than one shared screen. Each
gets the full panel, and it yields one more rain transition.

## Targets come from the firmware's own drawing code

A scene's target is a `bool[TOTAL_PIXELS]` mask, produced by:

1. `Screen.clear()`
2. call the existing helper (`drawBigNumbers`, `drawWeather`, …)
3. read the buffer back via `Screen.getBufferIndex(i)`, recording `> 0`
4. `Screen.clear()`

No glyph data is duplicated. The digits and icons are pixel-identical to Big
Clock and Weather, and stay identical if those fonts ever change. This is the
central design decision; everything else is animation on top of it.

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

**DISSOLVE.** Locked pixels release progressively from the top row down. A
released pixel becomes a falling drop that moves down one row per tick until it
leaves the panel.

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

One full cycle is about 35 seconds.

## Shared weather store

Matrix Clock needs the temperature and icon that `WeatherPlugin` already fetches
and caches privately. Rather than a second wttr.in request, the fetch and cache
move into a small shared store:

```cpp
struct WeatherReading {
  int temperatureC;
  int icon;      // index into weatherIcons
  int iconY;
  int tempY;
  bool valid;
};
```

`WeatherStore` exposes `update()` (rate-limited HTTP fetch and parse),
`get()`, and `hasData()`. `WeatherPlugin` is refactored to populate it and
render from it; Matrix Clock reads it. Its on-device behaviour must not change.

wttr.in needs no API key — the request is
`https://wttr.in/{location}?format=j2&lang=en`. Sharing is about one update
schedule and one copy of the parsing code, not about API cost.

In the simulator this comes free: the store goes through the same stubbed
`HTTPClient`, so both plugins see the canned forecast and `w` cycles conditions
for both.

If the store has no reading yet, the weather scenes are skipped and the cycle
runs time-only until data arrives.

## Files

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
- Scenes advance in order and wrap: time → icon → temp → time.
- DISSOLVE eventually clears every locked pixel.
- With no weather reading, the cycle runs time-only and never stalls.
- A full cycle runs without crashing or writing out of bounds.
- The locked frame at each rotation equals the rotation of the frame at
  rotation 0, confirming the plugin adds no rotation of its own.

## Out of scope

- 12-hour format, seconds, or a date scene.
- Configuring timings or scene order from the web UI.
- Changing `MatrixRainPlugin`; the rain code is re-implemented inside the new
  plugin because its needs differ (dim rendering, locking, dissolve). Extracting
  a shared rain engine for two plugins with different requirements would couple
  them for little gain.
