# Simulator

Renders the 16×16 LED matrix in a terminal, so effects can be previewed and
developed without flashing hardware.

It compiles the *real* firmware sources — `src/screen.cpp`, `src/signs.cpp`,
`src/PluginManager.cpp` and `src/plugins/*.cpp` — against a host shim for the
Arduino API in `sim/shim/`. Effects therefore behave exactly as they do on the
device, including rotation, brightness scaling and every drawing helper. There
is no second implementation to drift out of sync.

## Build and run

```sh
make -C sim
./sim/build/obegraensad-sim
```

Needs a C++17 compiler and a truecolor terminal. No other dependencies — no
CMake, no package manager, no third-party libraries.

## Keys

| Key       | Action                  |
|-----------|-------------------------|
| `j` / `k` | next / previous effect  |
| `r`       | rotate 0°/90°/180°/270° |
| `+` / `-` | global brightness       |
| `space`   | pause / resume          |
| `s`       | single-step one frame   |
| `[` / `]` | clock speed 0.25×–4×    |
| `w`       | cycle weather condition |
| `b`       | press the lamp's button |
| `q`       | quit                    |

Rotation matters if you are deciding how to hang the panel: it runs the
firmware's own `Screen_::rotate()`, so what you see is what the device does.

## Headless mode

Print frames as text, for tests or for diffing an effect's output:

```sh
./sim/build/obegraensad-sim --plugin Wave --frames 3 --dump
./sim/build/obegraensad-sim --plugin "Big Clock" --frames 1 --dump --rotate 1
```

`--verbose` sends the firmware's `Serial` output to stderr. It is suppressed by
default because it would corrupt the display.

## Tests

```sh
make -C sim test
```

Covers the simulated clock, the rotation and brightness mapping against the
firmware's own transform, the ASCII dump, the renderer's escape sequences, key
handling, and a smoke test that every registered plugin survives setup plus 30
frames.

## Developing an effect

Add the plugin under `src/plugins/`, register it in `sim/host/sim_registry.cpp`
(and in `src/main.cpp` for the firmware), then:

```sh
make -C sim && ./sim/build/obegraensad-sim
```

## How the firmware stays untouched

Every change to `src/` and `include/` is wrapped in `#ifdef SIMULATOR`, so the
device build is byte-for-byte unaffected. The guarded sites are:

- `include/constants.h` — skip `ENABLE_SERVER` / `ENABLE_STORAGE`, keep the
  timezone defines.
- `include/screen.h` — expose a public `render()` that calls the private
  `_render()`.
- `include/websocket.h` — declare a no-op `sendWSMessage`.
- `include/plugins/WeatherPlugin.h` — include the host HTTP stubs.
- `src/screen.cpp` — publish the frame instead of clocking it out over SPI.

Two unguarded changes were also needed, both fixes rather than simulator
scaffolding: `case this->CONSTANT:` in `BreakoutPlugin` and `GameoflifePlugin`
is not a constant expression under clang, so the redundant `this->` was dropped.
The firmware behaviour is identical.

## Limitations

- `DDPPlugin` and `ArtNetPlugin` are excluded: they render whatever an external
  controller pushes over UDP, so there is nothing of their own to preview.
- `WeatherPlugin` uses a canned forecast; `w` cycles every condition so all the
  icons can be checked.
- `DrawPlugin` and `AnimationPlugin` are driven from the web UI over websockets,
  so they show their static placeholder here.
- The clock plugins read your host's local time.
