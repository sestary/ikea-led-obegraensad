# TUI Simulator — Design

**Date:** 2026-08-10
**Status:** Approved, ready for implementation planning

## Problem

The 30 effect plugins in this repo can only be seen by flashing an ESP32 and
wiring it into an OBEGRÄNSAD panel. That blocks two things: deciding whether the
hack is worth doing before buying hardware, and iterating on a new effect
without a flash cycle per change.

## Goal

A terminal simulator that renders the firmware's 16×16 LED matrix, so effects
can be browsed and authored on a laptop.

Two use cases, both first-class:

1. **Browse** the built-in effects to evaluate the project before committing to hardware.
2. **Author** new effects with a fast edit–build–see loop.

## Core principle

Compile the **real** plugin sources natively. The simulator links
`src/plugins/*.cpp` and `src/screen.cpp` as-is against a small host shim.

The alternative — porting effects to another language — was rejected. A ported
simulator drifts from the firmware silently, and a preview you can't trust is
worse than no preview.

## Why this is cheap here

The firmware is already structured for it:

- All drawing funnels through one singleton, `Screen`, on a `ROWS × COLS` buffer.
- Hardware access in `screen.cpp` is confined to `setup()`, `_render()`, and
  `onScreenTimer()`. The other ~450 lines are pure math.
- `ENABLE_SERVER` and `ENABLE_STORAGE` already guard every `ESPAsyncWebServer`
  and `Preferences` dependency. Leaving both undefined drops the entire ESP
  networking and flash stack without touching a line.
- Plugin dependencies on Arduino are shallow: `random` (54 uses), `Serial` (32),
  `millis` (12), `delay` (10).

## Architecture

```
sim/
  CMakeLists.txt          native build; compiles ../src/**, ../include/**
  shim/
    Arduino.h  Arduino.cpp    millis, delay, random, Serial, String,
                              getLocalTime, no-op pinMode/digitalWrite
    WiFi.h  HTTPClient.h      stubs so WeatherPlugin links
  host/
    frame.h  frame.cpp        frame snapshot handed from Screen to the TUI
  tui/
    render.cpp                half-block truecolor drawing
    input.cpp                 raw-mode keyboard handling
  main_sim.cpp                registers plugins, runs the frame loop
```

`ArduinoJson` is fetched by CMake `FetchContent` (single header) to satisfy
`Plugin::websocketHook`.

### Upstream diff

Roughly 10 lines, every one `#ifdef SIMULATOR`-guarded, matching the
`#ifdef ENABLE_STORAGE` / `#ifdef ESP32` style already in the codebase:

- `src/screen.cpp` — guard the hardware bodies of `setup()`, `_render()`, and
  `onScreenTimer()` (SPI init, pin config, timer interrupts).
- `include/screen.h` — move `getRotatedRenderBuffer()` from private to public.

Everything else compiles unmodified, including all of `signs.cpp` and
`PluginManager.cpp`. Keeping the diff this small matters: this is a fork, and it
should stay mergeable with upstream.

## Rendering

Half-block truecolor. Each `▀` carries two LEDs — foreground is the upper LED,
background the lower — giving square-ish pixels in 16×8 terminal cells.

Brightness is the LED's own value scaled by `Screen`'s global brightness, mapped
onto a warm-white ramp so the output reads like the physical lamp, which is
single-color and brightness-only.

`COLS` and `ROWS` are read from `constants.h`. Nothing hardcodes 16, so the
simulator stays correct if the define ever changes.

### Orientation

The panel can hang landscape or portrait, and the firmware already models this:
`currentRotation` (0–3) is applied by `Screen_::rotate()` at render time.

The simulator pulls frames through `getRotatedRenderBuffer()` — the same call
the hardware render path makes — so orientation is the firmware's own rotation
code, not a lookalike. `r` cycles 0°/90°/180°/270°.

### Layout

```
 IKEA OBEGRÄNSAD — simulator          Plugins
 ▀▀▄▄░░  ▒▒▓▓██▓▓▒▒░░                   Snake
 ▄▄██▓▓▒▒░░  ░░▒▒▓▓██                   Stars
 ██▓▓▒▒░░    ░░▒▒▓▓██                 > Wave
 ▓▓▒▒░░  ▄▄▀▀░░▒▒▓▓██                   Spiral
 ...                                    Radar

 Wave   rot 0°   bri 255   58fps   1.0×
 j/k plugin  r rotate  +/- bright  space pause  s step  [/] speed  q quit
```

### Keys

| Key       | Action                          |
|-----------|---------------------------------|
| `j` / `k` | previous / next plugin          |
| `r`       | cycle rotation                  |
| `+` / `-` | global brightness               |
| `space`   | pause / resume                  |
| `s`       | single-step one frame           |
| `[` / `]` | clock speed 0.25×–4×            |
| `w`       | cycle weather condition         |
| `q`       | quit                            |

## Timing

The shim's `millis()` is backed by a controllable clock: real monotonic time
multiplied by a speed factor, freezable for pause and advanceable by a fixed
step. Effects run at true hardware speed by default.

This costs almost nothing — the clock is the only time source plugins have, via
`millis()` and `NonBlockingDelay` — and it buys pause, single-step, and
slow-motion for inspecting fast effects, plus deterministic tests.

## Network and time plugins

Six of 30 plugins reach outside the display:

| Plugin                                          | Dependency      | Handling                       |
|-------------------------------------------------|-----------------|--------------------------------|
| `ClockPlugin`, `BigClockPlugin`, `PongClockPlugin`, `TickingClockPlugin` | `getLocalTime`  | shim reads host time           |
| `WeatherPlugin`                                 | `WiFi`, `HTTPClient` | canned forecast, `w` cycles conditions |
| `DDPPlugin`, `ArtNetPlugin`                     | UDP             | excluded                       |

Clocks work unmodified against host time. Weather gets a canned response, and
cycling every condition on a keypress is more useful for previewing icons than
real weather would be.

DDP and ArtNet are inbound protocols — they render whatever an external
controller pushes, so they have no effect of their own to preview. Excluding
them also avoids shimming UDP sockets.

That leaves **28 of 30 plugins previewable**.

## Testing

A headless mode makes this testable rather than eyeball-only:

```
obegraensad-sim --plugin Wave --frames 60 --dump
```

`--dump` writes frames as plain text (one character per LED brightness bucket),
which supports real assertions:

- `CheckerboardPlugin` produces the exact expected alternating pattern.
- Rotation by 90° maps `(x, y)` → `(y, COLS-1-x)`.
- Global brightness scales every LED proportionally.
- Every registered plugin runs N frames without crash or out-of-bounds write.

Tests drive the shim clock directly instead of sleeping, so they are
deterministic and fast.

## Development loop

```sh
cmake -B sim/build -S sim
cmake --build sim/build && ./sim/build/obegraensad-sim
```

Documented as a one-liner. No file watcher — that is a tool preference better
left to the user's own setup than baked into the repo.

## Risks

`DrawPlugin` and `AnimationPlugin` consume websocket data and have not yet been
compile-tested with `ENABLE_SERVER` undefined. If they do not build cleanly they
get a small `#ifdef` guard, or join the excluded list. This surfaces on the
first build.

## Out of scope

- Simulating the button, OTA, or the web UI.
- RGB or any color model beyond the panel's real single-color brightness.
- A file watcher or hot reload.
- Changes to the firmware's behaviour. The simulator observes; it does not
  redesign.
