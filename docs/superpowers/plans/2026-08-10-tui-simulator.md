# TUI Simulator Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A terminal simulator that renders the firmware's 16×16 LED matrix by compiling the real plugin sources natively, so effects can be previewed and authored without flashing hardware.

**Architecture:** A `sim/` tree containing a host shim for the Arduino API, a frame-snapshot handoff, and a TUI renderer. The firmware's own `src/screen.cpp`, `src/signs.cpp`, `src/PluginManager.cpp` and `src/plugins/*.cpp` are compiled unmodified except for `#ifdef SIMULATOR` guards on the four hardware sites. The simulator therefore runs the same drawing, rotation and effect code the device runs.

**Tech Stack:** C++17, Apple clang, plain GNU Make. No third-party libraries, no package manager.

## Global Constraints

- **No CMake.** `cmake` is not installed on this machine. Build with a plain `Makefile`.
- **No third-party dependencies.** Every shim header is hand-written in `sim/shim/`. Do not vendor or download ArduinoJson.
- **C++ standard: `-std=gnu++17`.** The firmware uses GNU extensions.
- **`COLS`/`ROWS` come from `include/constants.h`.** Never hardcode 16 anywhere in `sim/`.
- **Upstream files stay minimally modified.** Changes to `src/` and `include/` must be wrapped in `#ifdef SIMULATOR` / `#ifndef SIMULATOR`. This is a fork and must stay mergeable.
- **Excluded plugins:** `DDPPlugin`, `ArtNetPlugin`. They are inbound UDP protocols with nothing of their own to render. All other 28 are included.
- **Panel is single-color.** Brightness only, 0–255. No RGB anywhere.

---

### Task 1: Build skeleton, Arduino shim, controllable clock

Establishes the native build and proves firmware sources compile against a host shim.

**Files:**
- Create: `sim/Makefile`
- Create: `sim/shim/Arduino.h`, `sim/shim/Arduino.cpp`
- Create: `sim/shim/sim_clock.h`, `sim/shim/sim_clock.cpp`
- Create: `sim/tests/test_main.h`, `sim/tests/test_clock.cpp`
- Modify: `include/constants.h` (guard `ENABLE_SERVER` / `ENABLE_STORAGE`)
- Modify: `src/screen.cpp` (guard `setBrightness` pin writes)

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `unsigned long millis()` — simulated milliseconds.
  - `void simClockSetSpeed(double factor)` — 0.25–4.0.
  - `double simClockGetSpeed()`
  - `void simClockSetPaused(bool)` / `bool simClockIsPaused()`
  - `void simClockStep(unsigned long ms)` — advance while paused.

- [ ] **Step 1: Write the failing test**

Create `sim/tests/test_main.h`:

```cpp
#pragma once
#include <cstdio>
#include <cstdlib>

static int g_failures = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
      g_failures++;                                                            \
    }                                                                          \
  } while (0)

#define CHECK_EQ(a, b)                                                         \
  do {                                                                         \
    auto _a = (a);                                                             \
    auto _b = (b);                                                             \
    if (!(_a == _b)) {                                                         \
      std::printf("FAIL %s:%d: %s == %s (got %ld vs %ld)\n", __FILE__,         \
                  __LINE__, #a, #b, (long)_a, (long)_b);                       \
      g_failures++;                                                            \
    }                                                                          \
  } while (0)

#define RUN(fn)                                                                \
  do {                                                                         \
    std::printf("run %s\n", #fn);                                              \
    fn();                                                                      \
  } while (0)

#define TEST_MAIN_END                                                          \
  do {                                                                         \
    if (g_failures) {                                                          \
      std::printf("\n%d FAILURE(S)\n", g_failures);                            \
      return 1;                                                                \
    }                                                                          \
    std::printf("\nall tests passed\n");                                       \
    return 0;                                                                  \
  } while (0)
```

Create `sim/tests/test_clock.cpp`:

```cpp
#include "test_main.h"
#include "Arduino.h"
#include "sim_clock.h"

static void test_paused_clock_does_not_advance() {
  simClockSetPaused(true);
  unsigned long a = millis();
  for (volatile int i = 0; i < 1000000; i++) {
  }
  CHECK_EQ(millis(), a);
}

static void test_step_advances_exactly() {
  simClockSetPaused(true);
  unsigned long a = millis();
  simClockStep(50);
  CHECK_EQ(millis(), a + 50);
  simClockStep(10);
  CHECK_EQ(millis(), a + 60);
}

static void test_speed_multiplies_elapsed_time() {
  simClockSetPaused(true);
  simClockSetSpeed(1.0);
  unsigned long a = millis();
  simClockStep(100);
  CHECK_EQ(millis(), a + 100);
  // Stepping is in simulated ms, so speed must not double-apply to steps.
  simClockSetSpeed(2.0);
  simClockStep(100);
  CHECK_EQ(millis(), a + 200);
}

static void test_speed_roundtrips() {
  simClockSetSpeed(0.25);
  CHECK(simClockGetSpeed() == 0.25);
  simClockSetSpeed(4.0);
  CHECK(simClockGetSpeed() == 4.0);
}

int main() {
  RUN(test_paused_clock_does_not_advance);
  RUN(test_step_advances_exactly);
  RUN(test_speed_multiplies_elapsed_time);
  RUN(test_speed_roundtrips);
  TEST_MAIN_END;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make -C sim test`
Expected: FAIL — no Makefile yet, `make: *** No rule to make target 'test'`.

- [ ] **Step 3: Write the clock**

Create `sim/shim/sim_clock.h`:

```cpp
#pragma once

// Simulated time source backing millis(). Real monotonic time scaled by a
// speed factor, freezable for pause and advanceable by a fixed step.
void simClockSetSpeed(double factor);
double simClockGetSpeed();
void simClockSetPaused(bool paused);
bool simClockIsPaused();
void simClockStep(unsigned long ms);
unsigned long simClockNow();
```

Create `sim/shim/sim_clock.cpp`:

```cpp
#include "sim_clock.h"
#include <chrono>

namespace {
using Clock = std::chrono::steady_clock;

Clock::time_point g_lastReal = Clock::now();
double g_simMs = 0.0;
double g_speed = 1.0;
bool g_paused = false;

void accumulate() {
  auto now = Clock::now();
  if (!g_paused) {
    double realMs =
        std::chrono::duration<double, std::milli>(now - g_lastReal).count();
    g_simMs += realMs * g_speed;
  }
  g_lastReal = now;
}
} // namespace

void simClockSetSpeed(double factor) {
  accumulate();
  if (factor < 0.25)
    factor = 0.25;
  if (factor > 4.0)
    factor = 4.0;
  g_speed = factor;
}

double simClockGetSpeed() { return g_speed; }

void simClockSetPaused(bool paused) {
  accumulate();
  g_paused = paused;
}

bool simClockIsPaused() { return g_paused; }

void simClockStep(unsigned long ms) {
  accumulate();
  g_simMs += static_cast<double>(ms);
}

unsigned long simClockNow() {
  accumulate();
  return static_cast<unsigned long>(g_simMs);
}
```

- [ ] **Step 4: Write the Arduino shim**

Create `sim/shim/Arduino.h`:

```cpp
#pragma once

// Host stand-in for the Arduino core. Only the surface the firmware's
// screen/signs/plugin sources actually touch.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

#define IRAM_ATTR
#define DRAM_ATTR
#define PROGMEM

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif

#define OUTPUT 1
#define INPUT 0
#define INPUT_PULLUP 2
#define HIGH 1
#define LOW 0

typedef uint8_t byte;

using std::abs;
using std::max;
using std::min;

template <class T, class L, class H> T constrain(T v, L lo, H hi) {
  return v < static_cast<T>(lo)   ? static_cast<T>(lo)
         : v > static_cast<T>(hi) ? static_cast<T>(hi)
                                  : v;
}

unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned long us);

long random(long howbig);
long random(long howsmall, long howbig);
void randomSeed(unsigned long seed);

void pinMode(int pin, int mode);
void digitalWrite(int pin, int value);
int digitalRead(int pin);

// Host wall-clock, so the clock plugins render the real local time.
bool getLocalTime(struct tm *info, uint32_t ms = 5000);
void configTime(long gmtOffset, int daylightOffset, const char *server);
void configTzTime(const char *tz, const char *server);

// Arduino's String is close enough to std::string for the firmware's usage.
using String = std::string;

// Serial output would corrupt the TUI, so it is dropped unless the simulator
// is started with --verbose (see simSerialSetVerbose).
void simSerialSetVerbose(bool verbose);
bool simSerialVerbose();

struct SerialShim {
  void begin(unsigned long) {}
  void flush() {}
  explicit operator bool() const { return true; }

  void print(const std::string &s) { emit("%s", s.c_str()); }
  void print(const char *s) { emit("%s", s); }
  void print(char c) { emit("%c", c); }
  void print(int v) { emit("%d", v); }
  void print(unsigned v) { emit("%u", v); }
  void print(long v) { emit("%ld", v); }
  void print(unsigned long v) { emit("%lu", v); }
  void print(double v) { emit("%f", v); }

  template <class T> void println(T v) {
    print(v);
    emit("\n");
  }
  void println() { emit("\n"); }

  template <class... A> void printf(const char *fmt, A... args) {
    if (simSerialVerbose())
      std::fprintf(stderr, fmt, args...);
  }

private:
  template <class... A> void emit(const char *fmt, A... args) {
    if (simSerialVerbose())
      std::fprintf(stderr, fmt, args...);
  }
};

extern SerialShim Serial;
```

Create `sim/shim/Arduino.cpp`:

```cpp
#include "Arduino.h"
#include "sim_clock.h"

SerialShim Serial;

namespace {
bool g_verbose = false;
}

void simSerialSetVerbose(bool verbose) { g_verbose = verbose; }
bool simSerialVerbose() { return g_verbose; }

unsigned long millis() { return simClockNow(); }
unsigned long micros() { return simClockNow() * 1000UL; }

// Effects are driven by the simulator's frame loop, not by blocking waits.
// Advancing the simulated clock keeps NonBlockingDelay behaving sensibly.
void delay(unsigned long ms) { simClockStep(ms); }
void delayMicroseconds(unsigned long) {}

long random(long howbig) {
  if (howbig <= 0)
    return 0;
  return std::rand() % howbig;
}

long random(long howsmall, long howbig) {
  if (howbig <= howsmall)
    return howsmall;
  return howsmall + (std::rand() % (howbig - howsmall));
}

void randomSeed(unsigned long seed) { std::srand(static_cast<unsigned>(seed)); }

void pinMode(int, int) {}
void digitalWrite(int, int) {}
int digitalRead(int) { return 0; }

bool getLocalTime(struct tm *info, uint32_t) {
  std::time_t now = std::time(nullptr);
  localtime_r(&now, info);
  return true;
}

void configTime(long, int, const char *) {}
void configTzTime(const char *, const char *) {}
```

- [ ] **Step 5: Apply the SIMULATOR guards to upstream files**

In `include/constants.h`, replace:

```c
// disable if you do not want to have online functionality
#define ENABLE_SERVER
```

with:

```c
// disable if you do not want to have online functionality
// the native simulator build has no networking stack
#ifndef SIMULATOR
#define ENABLE_SERVER
#endif
```

and replace:

```c
#ifndef ESP8266
#define ENABLE_STORAGE
#endif
```

with:

```c
#if !defined(ESP8266) && !defined(SIMULATOR)
#define ENABLE_STORAGE
#endif
```

In `src/screen.cpp`, inside `Screen_::setBrightness`, replace:

```c
#ifndef ESP8266
  pinMode(PIN_ENABLE, OUTPUT);
  digitalWrite(PIN_ENABLE, LOW);
#endif
```

with:

```c
#if !defined(ESP8266) && !defined(SIMULATOR)
  pinMode(PIN_ENABLE, OUTPUT);
  digitalWrite(PIN_ENABLE, LOW);
#endif
```

Note: `Screen_::setup()`'s hardware is already inside `#ifdef ESP32` / `#ifdef ESP8266`, both undefined in the native build. It needs no guard.

- [ ] **Step 6: Write the Makefile**

Create `sim/Makefile`:

```make
# Native simulator build. No CMake, no third-party dependencies.

ROOT      := ..
CXX       ?= c++
CXXFLAGS  := -std=gnu++17 -O2 -Wall -DSIMULATOR \
             -I$(ROOT)/include -Ishim -Ihost -Itui -I.
BUILD     := build

FIRMWARE_SRC := $(ROOT)/src/screen.cpp \
                $(ROOT)/src/signs.cpp \
                $(ROOT)/src/PluginManager.cpp
PLUGIN_SRC   := $(filter-out %/DDPPlugin.cpp %/ArtNet.cpp, \
                  $(wildcard $(ROOT)/src/plugins/*.cpp))
SIM_SRC      := $(wildcard shim/*.cpp) $(wildcard host/*.cpp) \
                $(wildcard tui/*.cpp)

CORE_SRC := $(FIRMWARE_SRC) $(PLUGIN_SRC) $(SIM_SRC)
CORE_OBJ := $(patsubst %.cpp,$(BUILD)/%.o,$(subst ../,root_/,$(CORE_SRC)))

BIN := $(BUILD)/obegraensad-sim

.PHONY: all test clean run
all: $(BIN)

$(BUILD)/root_/%.o: $(ROOT)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BIN): $(CORE_OBJ) $(BUILD)/main_sim.o
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

run: $(BIN)
	$(BIN)

TEST_SRC := $(wildcard tests/*.cpp)
TEST_BIN := $(patsubst tests/%.cpp,$(BUILD)/tests/%,$(TEST_SRC))

$(BUILD)/tests/%: tests/%.cpp $(CORE_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Itests $< $(CORE_OBJ) -o $@

test: $(TEST_BIN)
	@for t in $(TEST_BIN); do echo "== $$t"; $$t || exit 1; done

clean:
	rm -rf $(BUILD)
```

Note: `main_sim.cpp` and the `host/`, `tui/` directories arrive in later tasks. Until then, create empty placeholder dirs so the wildcards resolve, and build only the test target.

- [ ] **Step 7: Run tests to verify they pass**

Run: `make -C sim test`
Expected: `test_clock` runs 4 tests, prints `all tests passed`, exit 0.

- [ ] **Step 8: Commit**

```bash
git add sim/Makefile sim/shim sim/tests include/constants.h src/screen.cpp
git commit -m "feat(sim): native build skeleton, Arduino shim and simulated clock"
```

---

### Task 2: Frame handoff and rotation

Makes the firmware's render path deliver frames to the host, and locks down that rotation and brightness match the device.

**Files:**
- Create: `sim/host/frame.h`, `sim/host/frame.cpp`
- Create: `sim/tests/test_frame.cpp`
- Modify: `src/screen.cpp` (SIMULATOR branch in `_render()`, include guard)

**Interfaces:**
- Consumes: `millis()` from Task 1.
- Produces:
  - `void simPublishFrame(const uint8_t *buf, uint8_t globalBrightness)` — called by `Screen_::_render()`.
  - `struct SimFrame { uint8_t px[TOTAL_PIXELS]; unsigned long seq; }`
  - `SimFrame simLatestFrame()` — copy of the most recent frame.
  - `void simRenderTick()` — drives one `Screen_::_render()` via the public `Screen.render()` shim below.

- [ ] **Step 1: Write the failing test**

Create `sim/tests/test_frame.cpp`:

```cpp
#include "test_main.h"
#include "Arduino.h"
#include "constants.h"
#include "frame.h"
#include "screen.h"

static void test_pixel_reaches_frame_unrotated() {
  Screen.setCurrentRotation(0);
  Screen.setBrightness(MAX_BRIGHTNESS);
  Screen.clear();
  Screen.setPixel(3, 5, 1, MAX_BRIGHTNESS);
  simRenderTick();
  SimFrame f = simLatestFrame();
  CHECK_EQ((int)f.px[5 * COLS + 3], (int)MAX_BRIGHTNESS);
}

static void test_rotation_90_maps_x_y_to_y_cols_minus_1_minus_x() {
  Screen.setCurrentRotation(1);
  Screen.setBrightness(MAX_BRIGHTNESS);
  Screen.clear();
  Screen.setPixel(3, 5, 1, MAX_BRIGHTNESS);
  simRenderTick();
  SimFrame f = simLatestFrame();
  // rotate() writes temp[col * COLS + (ROWS - 1 - row)] = src[row * COLS + col]
  CHECK_EQ((int)f.px[3 * COLS + (ROWS - 1 - 5)], (int)MAX_BRIGHTNESS);
}

static void test_rotation_180_maps_to_opposite_corner() {
  Screen.setCurrentRotation(2);
  Screen.setBrightness(MAX_BRIGHTNESS);
  Screen.clear();
  Screen.setPixel(0, 0, 1, MAX_BRIGHTNESS);
  simRenderTick();
  SimFrame f = simLatestFrame();
  CHECK_EQ((int)f.px[TOTAL_PIXELS - 1], (int)MAX_BRIGHTNESS);
}

static void test_global_brightness_scales_frame() {
  Screen.setCurrentRotation(0);
  Screen.setBrightness(128);
  Screen.clear();
  Screen.setPixel(0, 0, 1, MAX_BRIGHTNESS);
  simRenderTick();
  SimFrame f = simLatestFrame();
  CHECK_EQ((int)f.px[0], (255 * 128) / 255);
}

static void test_sequence_number_increments() {
  unsigned long a = simLatestFrame().seq;
  simRenderTick();
  CHECK(simLatestFrame().seq > a);
}

int main() {
  Screen.setup();
  RUN(test_pixel_reaches_frame_unrotated);
  RUN(test_rotation_90_maps_x_y_to_y_cols_minus_1_minus_x);
  RUN(test_rotation_180_maps_to_opposite_corner);
  RUN(test_global_brightness_scales_frame);
  RUN(test_sequence_number_increments);
  TEST_MAIN_END;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make -C sim test`
Expected: FAIL — `'frame.h' file not found`.

- [ ] **Step 3: Write the frame handoff**

Create `sim/host/frame.h`:

```cpp
#pragma once

#include "constants.h"
#include <cstdint>

struct SimFrame {
  uint8_t px[TOTAL_PIXELS];
  unsigned long seq;
};

// Called by Screen_::_render() under -DSIMULATOR, in place of the SPI write.
void simPublishFrame(const uint8_t *buf, uint8_t globalBrightness);

// Latest published frame, copied.
SimFrame simLatestFrame();

// Drive one render pass. Used by the frame loop and by tests.
void simRenderTick();
```

Create `sim/host/frame.cpp`:

```cpp
#include "frame.h"
#include "screen.h"

namespace {
SimFrame g_frame{};
}

void simPublishFrame(const uint8_t *buf, uint8_t globalBrightness) {
  for (int i = 0; i < TOTAL_PIXELS; i++) {
    g_frame.px[i] =
        static_cast<uint8_t>((static_cast<uint16_t>(buf[i]) * globalBrightness) /
                             MAX_BRIGHTNESS);
  }
  g_frame.seq++;
}

SimFrame simLatestFrame() { return g_frame; }

void simRenderTick() { Screen.render(); }
```

- [ ] **Step 4: Add the SIMULATOR branch to the firmware render path**

In `src/screen.cpp`, replace the include block:

```c
#include "screen.h"
#include "constants.h"
#include <SPI.h>
#include <algorithm>
```

with:

```c
#include "screen.h"
#include "constants.h"
#include <algorithm>

#ifdef SIMULATOR
#include "host/frame.h"
#else
#include <SPI.h>
#endif
```

Then in `Screen_::_render()`, immediately after the `const auto buf = ...` line, insert:

```c
#ifdef SIMULATOR
  // The simulator has no shift registers to clock out to. Hand the rotated,
  // brightness-scaled frame to the TUI instead of PWM-dithering it over SPI.
  simPublishFrame(buf, brightness_);
#else
```

and close the branch by changing the tail of `_render()` from:

```c
#ifdef ESP8266
  timer1_write(100);
#endif
}
```

to:

```c
#ifdef ESP8266
  timer1_write(100);
#endif
#endif // SIMULATOR
}
```

`_render()` is private, so add a public entry point. In `include/screen.h`, in the `public:` section after `void setup();`, add:

```cpp
#ifdef SIMULATOR
  // Lets the simulator drive a render pass; on hardware this is the timer ISR.
  void render() { _render(); }
#endif
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `make -C sim test`
Expected: `test_frame` runs 5 tests, `all tests passed`.

- [ ] **Step 6: Commit**

```bash
git add sim/host sim/tests/test_frame.cpp src/screen.cpp include/screen.h
git commit -m "feat(sim): publish rotated frames from the firmware render path"
```

---

### Task 3: Complete the shim so all 28 plugins compile

**Files:**
- Create: `sim/shim/WiFi.h`, `sim/shim/WiFiClientSecure.h`, `sim/shim/HTTPClient.h`, `sim/shim/HTTPClient.cpp`, `sim/shim/ArduinoJson.h`, `sim/shim/ESPmDNS.h`, `sim/shim/sim_weather.h`
- Create: `sim/tests/test_plugins.cpp`
- Modify: `sim/Makefile` if `src/config.cpp` proves necessary

**Interfaces:**
- Consumes: `Arduino.h` from Task 1, `frame.h` from Task 2.
- Produces:
  - `void simSetWeather(int tempC, int weatherCode)`
  - `int simWeatherCount()` — number of canned conditions.
  - `void simCycleWeather()` — advance to the next canned condition.

- [ ] **Step 1: Write the failing test**

Create `sim/tests/test_plugins.cpp`:

```cpp
#include "test_main.h"
#include "Arduino.h"
#include "PluginManager.h"
#include "frame.h"
#include "screen.h"
#include "sim_clock.h"
#include "sim_registry.h"

// Every registered plugin must survive setup + frames + teardown without
// crashing or writing outside the buffer.
static void test_all_plugins_run_without_crashing() {
  simRegisterPlugins();
  CHECK(pluginManager.getNumPlugins() == 28);
  for (Plugin *p : pluginManager.getAllPlugins()) {
    pluginManager.setActivePluginById(p->getId());
    pluginManager.setupActivePlugin();
    for (int i = 0; i < 30; i++) {
      pluginManager.runActivePlugin();
      simRenderTick();
      simClockStep(33);
    }
  }
}

int main() {
  Screen.setup();
  RUN(test_all_plugins_run_without_crashing);
  TEST_MAIN_END;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make -C sim test`
Expected: FAIL — `'sim_registry.h' file not found` (delivered in Task 4), plus compile errors from `WiFi`, `HTTPClient`, `JsonDocument`, `getLocalTime`. Deliver the shim headers here; the registry lands in Task 4 and this test goes green then.

- [ ] **Step 3: Write the network shims**

Create `sim/shim/WiFi.h`:

```cpp
#pragma once
#include "Arduino.h"

#define WL_CONNECTED 3
#define WIFI_STA 1

struct WiFiClass {
  int status() const { return WL_CONNECTED; }
  String localIP() const { return "127.0.0.1"; }
  String macAddress() const { return "00:00:00:00:00:00"; }
  void mode(int) {}
  void begin(const char * = nullptr, const char * = nullptr) {}
};

extern WiFiClass WiFi;

struct WiFiClient {};
```

Create `sim/shim/WiFiClientSecure.h`:

```cpp
#pragma once
#include "WiFi.h"

struct WiFiClientSecure : WiFiClient {
  void setInsecure() {}
  void setTimeout(unsigned long) {}
};
```

Create `sim/shim/HTTPClient.h`:

```cpp
#pragma once
#include "Arduino.h"
#include "WiFiClientSecure.h"

#define HTTP_CODE_OK 200

// Serves a canned wttr.in-shaped payload so WeatherPlugin's real parsing,
// icon selection and drawing code all run unmodified.
struct HTTPClient {
  bool begin(WiFiClient &, const String &) { return true; }
  bool begin(const String &) { return true; }
  void setTimeout(unsigned long) {}
  int GET() { return HTTP_CODE_OK; }
  String getString();
  void end() {}
};
```

Create `sim/shim/sim_weather.h`:

```cpp
#pragma once

// Canned weather conditions, cycled from the TUI so every icon can be seen.
void simSetWeather(int tempC, int weatherCode);
int simWeatherTempC();
int simWeatherCode();
int simWeatherCount();
void simCycleWeather();
const char *simWeatherLabel();
```

Create `sim/shim/HTTPClient.cpp`:

```cpp
#include "HTTPClient.h"
#include "sim_weather.h"

namespace {
struct Condition {
  int tempC;
  int code;
  const char *label;
};

// wttr.in weather codes, one per icon branch in WeatherPlugin.
// Non-const: simSetWeather() pins slot 0 so tests can assert a known reading.
Condition kConditions[] = {
    {21, 113, "clear"},   {18, 116, "partly cloudy"}, {14, 122, "overcast"},
    {9, 296, "rain"},     {3, 338, "snow"},           {16, 200, "thunder"},
    {11, 248, "fog"},
};

int g_index = 0;
} // namespace

void simSetWeather(int tempC, int weatherCode) {
  kConditions[0].tempC = tempC;
  kConditions[0].code = weatherCode;
  g_index = 0;
}

int simWeatherTempC() { return kConditions[g_index].tempC; }
int simWeatherCode() { return kConditions[g_index].code; }
int simWeatherCount() {
  return static_cast<int>(sizeof(kConditions) / sizeof(kConditions[0]));
}
const char *simWeatherLabel() { return kConditions[g_index].label; }

void simCycleWeather() { g_index = (g_index + 1) % simWeatherCount(); }

String HTTPClient::getString() {
  // Payload shape is irrelevant: the stub JsonDocument answers from the
  // canned condition directly. Returned only so length logging works.
  return "{\"current_condition\":[{}]}";
}
```

- [ ] **Step 4: Write the JSON stub**

Create `sim/shim/ArduinoJson.h`:

```cpp
#pragma once
#include "Arduino.h"
#include "sim_weather.h"

// Minimal stand-in for ArduinoJson. WeatherPlugin's entire JSON surface is
// doc["current_condition"][0]["temp_C"].as<float>() and ["weatherCode"].as<int>(),
// so the proxy resolves those two keys from the canned condition and ignores
// the payload entirely.
struct JsonValue {
  const char *key = "";

  JsonValue operator[](const char *k) const { return JsonValue{k}; }
  JsonValue operator[](int) const { return JsonValue{key}; }

  template <class T> T as() const {
    if (std::strcmp(key, "temp_C") == 0)
      return static_cast<T>(simWeatherTempC());
    if (std::strcmp(key, "weatherCode") == 0)
      return static_cast<T>(simWeatherCode());
    return static_cast<T>(0);
  }
};

struct DeserializationError {
  explicit operator bool() const { return false; }
  const char *c_str() const { return "ok"; }
};

struct JsonDocument {
  JsonValue operator[](const char *k) const { return JsonValue{k}; }
};

inline DeserializationError deserializeJson(JsonDocument &, const String &) {
  return DeserializationError{};
}
```

Create `sim/shim/ESPmDNS.h` as an empty header guarded by `#pragma once`, in case `config.cpp` is pulled in.

- [ ] **Step 5: Build and fix the remaining symbol errors**

Run: `make -C sim 2>&1 | grep error: | head -20`

Work through whatever remains one at a time. Expected residue based on the spike: `byte`, `max`, `constrain` are already handled in `Arduino.h`. If `src/config.cpp` turns out to be required by `WeatherPlugin` (it calls `config.getWeatherLocation()`), add it to `FIRMWARE_SRC` in the Makefile and shim whatever it needs; if that proves large, replace the call site's dependency by adding a `sim/shim/sim_config.cpp` defining a minimal `config` object with `getWeatherLocation()` returning `WEATHER_LOCATION`.

- [ ] **Step 6: Commit**

```bash
git add sim/shim sim/tests/test_plugins.cpp sim/Makefile
git commit -m "feat(sim): shim WiFi, HTTP and JSON so every effect plugin builds"
```

---

### Task 4: Plugin registry and headless dump mode

**Files:**
- Create: `sim/host/sim_registry.h`, `sim/host/sim_registry.cpp`
- Create: `sim/host/dump.h`, `sim/host/dump.cpp`
- Create: `sim/main_sim.cpp`
- Create: `sim/tests/test_dump.cpp`

**Interfaces:**
- Consumes: `frame.h`, `sim_weather.h`, `PluginManager`.
- Produces:
  - `void simRegisterPlugins()` — registers all 28, in `main.cpp` order minus DDP/ArtNet.
  - `std::string simDumpFrame(const SimFrame &f)` — ROWS lines of COLS chars, ramp `" .:-=+*#%@"`.
  - `int simRunHeadless(const std::string &plugin, int frames)` — prints dumps, returns 0, or 2 if the plugin name is unknown.

- [ ] **Step 1: Write the failing test**

Create `sim/tests/test_dump.cpp`:

```cpp
#include "test_main.h"
#include "Arduino.h"
#include "PluginManager.h"
#include "dump.h"
#include "frame.h"
#include "screen.h"
#include "sim_registry.h"
#include <string>

static void test_dump_has_one_line_per_row() {
  Screen.setCurrentRotation(0);
  Screen.setBrightness(MAX_BRIGHTNESS);
  Screen.clear();
  simRenderTick();
  std::string d = simDumpFrame(simLatestFrame());
  int newlines = 0;
  for (char c : d)
    if (c == '\n')
      newlines++;
  CHECK_EQ(newlines, ROWS);
  CHECK_EQ((int)d.size(), ROWS * (COLS + 1));
}

static void test_dump_maps_off_and_full_brightness() {
  Screen.setCurrentRotation(0);
  Screen.setBrightness(MAX_BRIGHTNESS);
  Screen.clear();
  Screen.setPixel(0, 0, 1, MAX_BRIGHTNESS);
  simRenderTick();
  std::string d = simDumpFrame(simLatestFrame());
  CHECK_EQ(d[0], '@');
  CHECK_EQ(d[1], ' ');
}

static void test_checkerboard_alternates() {
  simRegisterPlugins();
  Screen.setCurrentRotation(0);
  Screen.setBrightness(MAX_BRIGHTNESS);
  pluginManager.setActivePlugin("Checkerboard");
  pluginManager.setupActivePlugin();
  pluginManager.runActivePlugin();
  simRenderTick();
  SimFrame f = simLatestFrame();
  // Adjacent cells in a checkerboard must differ.
  int differing = 0;
  for (int y = 0; y < ROWS; y++)
    for (int x = 0; x + 1 < COLS; x++)
      if (f.px[y * COLS + x] != f.px[y * COLS + x + 1])
        differing++;
  CHECK(differing > 0);
}

static void test_unknown_plugin_returns_error_code() {
  CHECK_EQ(simRunHeadless("NoSuchPlugin", 1), 2);
}

int main() {
  Screen.setup();
  RUN(test_dump_has_one_line_per_row);
  RUN(test_dump_maps_off_and_full_brightness);
  RUN(test_checkerboard_alternates);
  RUN(test_unknown_plugin_returns_error_code);
  TEST_MAIN_END;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make -C sim test`
Expected: FAIL — `'dump.h' file not found`.

- [ ] **Step 3: Write the registry**

Create `sim/host/sim_registry.h`:

```cpp
#pragma once

// Registers every previewable plugin. DDPPlugin and ArtNetPlugin are omitted:
// they render whatever an external controller pushes over UDP, so they have no
// effect of their own to preview. Safe to call more than once.
void simRegisterPlugins();
```

Create `sim/host/sim_registry.cpp`:

```cpp
#include "sim_registry.h"
#include "PluginManager.h"

#include "plugins/AnimationPlugin.h"
#include "plugins/BigClockPlugin.h"
#include "plugins/Blob.h"
#include "plugins/BreakoutPlugin.h"
#include "plugins/BubblesPlugin.h"
#include "plugins/CheckerboardPlugin.h"
#include "plugins/CirclePlugin.h"
#include "plugins/ClockPlugin.h"
#include "plugins/CometPlugin.h"
#include "plugins/DrawPlugin.h"
#include "plugins/FirefliesPlugin.h"
#include "plugins/FireworkPlugin.h"
#include "plugins/GameOfLifePlugin.h"
#include "plugins/LinesPlugin.h"
#include "plugins/MatrixRainPlugin.h"
#include "plugins/MeteorShowerPlugin.h"
#include "plugins/PongClockPlugin.h"
#include "plugins/RadarPlugin.h"
#include "plugins/RainPlugin.h"
#include "plugins/ScanlinesPlugin.h"
#include "plugins/SnakePlugin.h"
#include "plugins/SparkleFieldPlugin.h"
#include "plugins/SpiralPlugin.h"
#include "plugins/StarsPlugin.h"
#include "plugins/TickingClockPlugin.h"
#include "plugins/WaveBarsPlugin.h"
#include "plugins/WavePlugin.h"
#include "plugins/WeatherPlugin.h"

void simRegisterPlugins() {
  if (pluginManager.getNumPlugins() > 0)
    return;

  pluginManager.addPlugin(new DrawPlugin());
  pluginManager.addPlugin(new BreakoutPlugin());
  pluginManager.addPlugin(new SnakePlugin());
  pluginManager.addPlugin(new GameOfLifePlugin());
  pluginManager.addPlugin(new StarsPlugin());
  pluginManager.addPlugin(new LinesPlugin());
  pluginManager.addPlugin(new CirclePlugin());
  pluginManager.addPlugin(new RainPlugin());
  pluginManager.addPlugin(new MatrixRainPlugin());
  pluginManager.addPlugin(new FireworkPlugin());
  pluginManager.addPlugin(new BlobPlugin());
  pluginManager.addPlugin(new SpiralPlugin());
  pluginManager.addPlugin(new WavePlugin());
  pluginManager.addPlugin(new CheckerboardPlugin());
  pluginManager.addPlugin(new RadarPlugin());
  pluginManager.addPlugin(new BubblesPlugin());
  pluginManager.addPlugin(new CometPlugin());
  pluginManager.addPlugin(new FirefliesPlugin());
  pluginManager.addPlugin(new MeteorShowerPlugin());
  pluginManager.addPlugin(new ScanlinesPlugin());
  pluginManager.addPlugin(new SparkleFieldPlugin());
  pluginManager.addPlugin(new WaveBarsPlugin());
  pluginManager.addPlugin(new BigClockPlugin());
  pluginManager.addPlugin(new ClockPlugin());
  pluginManager.addPlugin(new PongClockPlugin());
  pluginManager.addPlugin(new TickingClockPlugin());
  pluginManager.addPlugin(new WeatherPlugin());
  pluginManager.addPlugin(new AnimationPlugin());
}
```

- [ ] **Step 4: Write the dump mode**

Create `sim/host/dump.h`:

```cpp
#pragma once
#include "frame.h"
#include <string>

// ROWS lines of COLS characters, brightness bucketed onto " .:-=+*#%@".
std::string simDumpFrame(const SimFrame &f);

// Runs one plugin headlessly and prints `frames` dumps. Returns 0 on success,
// 2 if the plugin name is not registered.
int simRunHeadless(const std::string &plugin, int frames);
```

Create `sim/host/dump.cpp`:

```cpp
#include "dump.h"
#include "PluginManager.h"
#include "constants.h"
#include "screen.h"
#include "sim_clock.h"
#include "sim_registry.h"
#include <cstdio>

namespace {
const char kRamp[] = " .:-=+*#%@";
const int kRampSize = static_cast<int>(sizeof(kRamp)) - 2; // exclude NUL
} // namespace

std::string simDumpFrame(const SimFrame &f) {
  std::string out;
  out.reserve(ROWS * (COLS + 1));
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      int v = f.px[y * COLS + x];
      out += kRamp[(v * kRampSize) / MAX_BRIGHTNESS];
    }
    out += '\n';
  }
  return out;
}

int simRunHeadless(const std::string &plugin, int frames) {
  simRegisterPlugins();

  Plugin *found = nullptr;
  for (Plugin *p : pluginManager.getAllPlugins())
    if (plugin == p->getName())
      found = p;

  if (!found) {
    std::fprintf(stderr, "unknown plugin: %s\navailable:\n", plugin.c_str());
    for (Plugin *p : pluginManager.getAllPlugins())
      std::fprintf(stderr, "  %s\n", p->getName());
    return 2;
  }

  pluginManager.setActivePluginById(found->getId());
  pluginManager.setupActivePlugin();

  for (int i = 0; i < frames; i++) {
    pluginManager.runActivePlugin();
    simRenderTick();
    std::printf("frame %d\n%s\n", i, simDumpFrame(simLatestFrame()).c_str());
    simClockStep(33);
  }
  return 0;
}
```

- [ ] **Step 5: Write the entry point**

Create `sim/main_sim.cpp`:

```cpp
#include "Arduino.h"
#include "constants.h"
#include "dump.h"
#include "screen.h"
#include <cstdio>
#include <cstring>
#include <string>

// screen.cpp and the plugins reference this global, which normally lives in
// src/main.cpp. The simulator does not compile main.cpp, so it is defined here.
volatile SYSTEM_STATUS currentStatus = NONE;

static void usage() {
  std::printf("usage: obegraensad-sim [--plugin NAME --frames N --dump] "
              "[--verbose]\n");
}

int main(int argc, char **argv) {
  std::string plugin;
  int frames = 1;
  bool dump = false;

  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--plugin" && i + 1 < argc)
      plugin = argv[++i];
    else if (a == "--frames" && i + 1 < argc)
      frames = std::atoi(argv[++i]);
    else if (a == "--dump")
      dump = true;
    else if (a == "--verbose")
      simSerialSetVerbose(true);
    else if (a == "--help") {
      usage();
      return 0;
    }
  }

  Screen.setup();

  if (dump) {
    if (plugin.empty()) {
      std::fprintf(stderr, "--dump requires --plugin NAME\n");
      return 2;
    }
    return simRunHeadless(plugin, frames);
  }

  // Interactive TUI arrives in Task 5.
  usage();
  return 0;
}
```

- [ ] **Step 6: Run tests to verify they pass**

Run: `make -C sim test`
Expected: `test_dump` and `test_plugins` both pass. `test_plugins` asserting 28 plugins now resolves.

- [ ] **Step 7: Verify headless mode by eye**

Run: `make -C sim && ./sim/build/obegraensad-sim --plugin Wave --frames 3 --dump`
Expected: three 16×16 ASCII frames showing a moving wave.

- [ ] **Step 8: Commit**

```bash
git add sim/host sim/main_sim.cpp sim/tests/test_dump.cpp
git commit -m "feat(sim): plugin registry and headless frame dump mode"
```

---

### Task 5: TUI renderer

**Files:**
- Create: `sim/tui/render.h`, `sim/tui/render.cpp`
- Create: `sim/tests/test_render.cpp`

**Interfaces:**
- Consumes: `SimFrame` from Task 2.
- Produces:
  - `std::string simRenderMatrix(const SimFrame &f)` — half-block body, ANSI truecolor, `ROWS/2` lines.
  - `std::string simRenderStatus(const char *plugin, int rotation, uint8_t brightness, double fps, double speed)`
  - `std::string simRenderSidebar(int selected, int firstVisible, int visibleRows)`

- [ ] **Step 1: Write the failing test**

Create `sim/tests/test_render.cpp`:

```cpp
#include "test_main.h"
#include "Arduino.h"
#include "constants.h"
#include "frame.h"
#include "render.h"
#include "screen.h"
#include <string>

static int countLines(const std::string &s) {
  int n = 0;
  for (char c : s)
    if (c == '\n')
      n++;
  return n;
}

static void test_matrix_uses_half_the_rows() {
  SimFrame f{};
  CHECK_EQ(countLines(simRenderMatrix(f)), ROWS / 2);
}

static void test_matrix_emits_half_block_glyphs() {
  SimFrame f{};
  f.px[0] = 255;
  std::string out = simRenderMatrix(f);
  CHECK(out.find("▀") != std::string::npos);
}

static void test_matrix_emits_truecolor_escapes() {
  SimFrame f{};
  f.px[0] = 255;
  std::string out = simRenderMatrix(f);
  CHECK(out.find("\033[38;2;") != std::string::npos);
  CHECK(out.find("\033[48;2;") != std::string::npos);
}

static void test_off_pixels_render_dark_not_black_text() {
  SimFrame f{};
  std::string out = simRenderMatrix(f);
  // An all-off frame must still emit a full grid, not an empty string.
  CHECK((int)out.size() > ROWS / 2);
}

static void test_status_line_reports_state() {
  std::string s = simRenderStatus("Wave", 90, 255, 60.0, 1.0);
  CHECK(s.find("Wave") != std::string::npos);
  CHECK(s.find("90") != std::string::npos);
  CHECK(s.find("255") != std::string::npos);
}

int main() {
  RUN(test_matrix_uses_half_the_rows);
  RUN(test_matrix_emits_half_block_glyphs);
  RUN(test_matrix_emits_truecolor_escapes);
  RUN(test_off_pixels_render_dark_not_black_text);
  RUN(test_status_line_reports_state);
  TEST_MAIN_END;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make -C sim test`
Expected: FAIL — `'render.h' file not found`.

- [ ] **Step 3: Write the renderer**

Create `sim/tui/render.h`:

```cpp
#pragma once
#include "frame.h"
#include <cstdint>
#include <string>

// Two LED rows per text row: foreground is the upper LED, background the
// lower, so pixels come out roughly square.
std::string simRenderMatrix(const SimFrame &f);

std::string simRenderStatus(const char *plugin, int rotation,
                            uint8_t brightness, double fps, double speed);

std::string simRenderSidebar(int selected, int firstVisible, int visibleRows);
```

Create `sim/tui/render.cpp`:

```cpp
#include "render.h"
#include "PluginManager.h"
#include "constants.h"
#include <cstdio>

namespace {
// The panel is single-color warm white. Map brightness onto a warm ramp so it
// reads like the lamp rather than a grey monitor.
void warmWhite(uint8_t v, int &r, int &g, int &b) {
  r = (v * 255) / 255;
  g = (v * 214) / 255;
  b = (v * 150) / 255;
}
} // namespace

std::string simRenderMatrix(const SimFrame &f) {
  std::string out;
  char buf[64];

  for (int y = 0; y < ROWS; y += 2) {
    for (int x = 0; x < COLS; x++) {
      int ur, ug, ub, lr, lg, lb;
      warmWhite(f.px[y * COLS + x], ur, ug, ub);
      warmWhite(f.px[(y + 1) * COLS + x], lr, lg, lb);
      std::snprintf(buf, sizeof(buf), "\033[38;2;%d;%d;%dm\033[48;2;%d;%d;%dm",
                    ur, ug, ub, lr, lg, lb);
      out += buf;
      out += "▀"; // upper half block
    }
    out += "\033[0m\n";
  }
  return out;
}

std::string simRenderStatus(const char *plugin, int rotation,
                            uint8_t brightness, double fps, double speed) {
  char buf[160];
  std::snprintf(buf, sizeof(buf),
                "%-14s rot %3d°   bri %3d   %4.0ffps   %.2f×", plugin,
                rotation, (int)brightness, fps, speed);
  return std::string(buf);
}

std::string simRenderSidebar(int selected, int firstVisible, int visibleRows) {
  std::string out;
  auto &plugins = pluginManager.getAllPlugins();
  for (int i = 0; i < visibleRows; i++) {
    int idx = firstVisible + i;
    if (idx >= (int)plugins.size())
      break;
    out += (idx == selected) ? "> " : "  ";
    out += plugins[idx]->getName();
    out += "\n";
  }
  return out;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `make -C sim test`
Expected: `test_render` runs 5 tests, all pass.

- [ ] **Step 5: Commit**

```bash
git add sim/tui sim/tests/test_render.cpp
git commit -m "feat(sim): half-block truecolor matrix renderer"
```

---

### Task 6: Interactive app loop and keyboard input

**Files:**
- Create: `sim/tui/input.h`, `sim/tui/input.cpp`
- Create: `sim/tui/app.h`, `sim/tui/app.cpp`
- Create: `sim/tests/test_app.cpp`
- Modify: `sim/main_sim.cpp` (call `simRunApp()` when not dumping)

**Interfaces:**
- Consumes: everything above.
- Produces:
  - `struct SimAppState { int selected; int rotation; uint8_t brightness; bool paused; bool quit; }`
  - `void simAppHandleKey(SimAppState &s, char key)` — pure state transition, unit-testable.
  - `int simRunApp()` — the interactive loop.

- [ ] **Step 1: Write the failing test**

Create `sim/tests/test_app.cpp`:

```cpp
#include "test_main.h"
#include "Arduino.h"
#include "app.h"
#include "constants.h"
#include "screen.h"
#include "sim_clock.h"
#include "sim_registry.h"

static SimAppState freshState() {
  SimAppState s;
  s.selected = 0;
  s.rotation = 0;
  s.brightness = MAX_BRIGHTNESS;
  s.paused = false;
  s.quit = false;
  return s;
}

static void test_r_cycles_rotation_through_four_values() {
  SimAppState s = freshState();
  simAppHandleKey(s, 'r');
  CHECK_EQ(s.rotation, 1);
  simAppHandleKey(s, 'r');
  simAppHandleKey(s, 'r');
  CHECK_EQ(s.rotation, 3);
  simAppHandleKey(s, 'r');
  CHECK_EQ(s.rotation, 0);
}

static void test_brightness_clamps_at_both_ends() {
  SimAppState s = freshState();
  for (int i = 0; i < 40; i++)
    simAppHandleKey(s, '+');
  CHECK_EQ((int)s.brightness, (int)MAX_BRIGHTNESS);
  for (int i = 0; i < 60; i++)
    simAppHandleKey(s, '-');
  CHECK_EQ((int)s.brightness, 0);
}

static void test_jk_moves_selection_and_clamps() {
  simRegisterPlugins();
  SimAppState s = freshState();
  simAppHandleKey(s, 'k');
  CHECK_EQ(s.selected, 0); // already at top
  simAppHandleKey(s, 'j');
  CHECK_EQ(s.selected, 1);
  for (int i = 0; i < 100; i++)
    simAppHandleKey(s, 'j');
  CHECK_EQ(s.selected, 27); // 28 plugins, last index
}

static void test_space_toggles_pause_and_q_quits() {
  SimAppState s = freshState();
  simAppHandleKey(s, ' ');
  CHECK(s.paused);
  simAppHandleKey(s, ' ');
  CHECK(!s.paused);
  simAppHandleKey(s, 'q');
  CHECK(s.quit);
}

static void test_brackets_change_clock_speed() {
  SimAppState s = freshState();
  simClockSetSpeed(1.0);
  simAppHandleKey(s, ']');
  CHECK(simClockGetSpeed() > 1.0);
  simAppHandleKey(s, '[');
  simAppHandleKey(s, '[');
  CHECK(simClockGetSpeed() < 1.0);
}

int main() {
  Screen.setup();
  RUN(test_r_cycles_rotation_through_four_values);
  RUN(test_brightness_clamps_at_both_ends);
  RUN(test_jk_moves_selection_and_clamps);
  RUN(test_space_toggles_pause_and_q_quits);
  RUN(test_brackets_change_clock_speed);
  TEST_MAIN_END;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make -C sim test`
Expected: FAIL — `'app.h' file not found`.

- [ ] **Step 3: Write raw-mode input**

Create `sim/tui/input.h`:

```cpp
#pragma once

// Puts the terminal in raw, non-blocking mode and restores it on exit.
void simInputBegin();
void simInputEnd();

// Returns the next pending key, or 0 if none is available.
char simInputPoll();
```

Create `sim/tui/input.cpp`:

```cpp
#include "input.h"
#include <cstdio>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace {
termios g_saved{};
bool g_active = false;
} // namespace

void simInputBegin() {
  if (g_active)
    return;
  tcgetattr(STDIN_FILENO, &g_saved);
  termios raw = g_saved;
  raw.c_lflag &= ~(ICANON | ECHO);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
  std::printf("\033[?25l");   // hide cursor
  std::printf("\033[?1049h"); // alternate screen
  std::fflush(stdout);
  g_active = true;
}

void simInputEnd() {
  if (!g_active)
    return;
  std::printf("\033[?1049l"); // leave alternate screen
  std::printf("\033[?25h");   // show cursor
  std::fflush(stdout);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_saved);
  g_active = false;
}

char simInputPoll() {
  char c = 0;
  ssize_t n = read(STDIN_FILENO, &c, 1);
  return n == 1 ? c : 0;
}
```

- [ ] **Step 4: Write the app**

Create `sim/tui/app.h`:

```cpp
#pragma once
#include <cstdint>

struct SimAppState {
  int selected;
  int rotation;
  uint8_t brightness;
  bool paused;
  bool quit;
};

// Pure state transition, so key handling is testable without a terminal.
void simAppHandleKey(SimAppState &s, char key);

// Interactive loop. Returns process exit code.
int simRunApp();
```

Create `sim/tui/app.cpp`:

```cpp
#include "app.h"
#include "Arduino.h"
#include "PluginManager.h"
#include "constants.h"
#include "frame.h"
#include "input.h"
#include "render.h"
#include "screen.h"
#include "sim_clock.h"
#include "sim_registry.h"
#include "sim_weather.h"
#include <chrono>
#include <cstdio>
#include <thread>

namespace {
const int kBrightnessStep = 8;
}

void simAppHandleKey(SimAppState &s, char key) {
  int count = (int)pluginManager.getNumPlugins();
  switch (key) {
  case 'j':
    if (count > 0 && s.selected < count - 1)
      s.selected++;
    break;
  case 'k':
    if (s.selected > 0)
      s.selected--;
    break;
  case 'r':
    s.rotation = (s.rotation + 1) & 0x3;
    break;
  case '+':
  case '=':
    s.brightness = (s.brightness > MAX_BRIGHTNESS - kBrightnessStep)
                       ? MAX_BRIGHTNESS
                       : s.brightness + kBrightnessStep;
    break;
  case '-':
    s.brightness = (s.brightness < kBrightnessStep) ? 0
                                                    : s.brightness - kBrightnessStep;
    break;
  case ' ':
    s.paused = !s.paused;
    simClockSetPaused(s.paused);
    break;
  case 's':
    simClockStep(33);
    break;
  case ']':
    simClockSetSpeed(simClockGetSpeed() * 2.0);
    break;
  case '[':
    simClockSetSpeed(simClockGetSpeed() / 2.0);
    break;
  case 'w':
    simCycleWeather();
    break;
  case 'q':
    s.quit = true;
    break;
  default:
    break;
  }
}

int simRunApp() {
  simRegisterPlugins();
  simInputBegin();

  SimAppState s{0, 0, MAX_BRIGHTNESS, false, false};
  int active = -1;
  double fps = 0.0;
  auto lastFrame = std::chrono::steady_clock::now();

  while (!s.quit) {
    for (char k = simInputPoll(); k != 0; k = simInputPoll())
      simAppHandleKey(s, k);

    if (s.selected != active) {
      auto &plugins = pluginManager.getAllPlugins();
      if (!plugins.empty()) {
        pluginManager.setActivePluginById(plugins[s.selected]->getId());
        pluginManager.setupActivePlugin();
      }
      active = s.selected;
    }

    Screen.setCurrentRotation(s.rotation);
    Screen.setBrightness(s.brightness);

    pluginManager.runActivePlugin();
    simRenderTick();

    auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - lastFrame).count();
    lastFrame = now;
    if (dt > 0)
      fps = 0.9 * fps + 0.1 * (1.0 / dt);

    const char *name = pluginManager.getActivePlugin()
                           ? pluginManager.getActivePlugin()->getName()
                           : "-";

    std::string out = "\033[H\033[2J";
    out += " IKEA OBEGRÄNSAD — simulator\n\n";
    out += simRenderMatrix(simLatestFrame());
    out += "\n ";
    out += simRenderStatus(name, s.rotation * 90, s.brightness, fps,
                           simClockGetSpeed());
    out += "\n\n";
    out += simRenderSidebar(s.selected, 0, (int)pluginManager.getNumPlugins());
    out += "\n j/k plugin  r rotate  +/- bright  space pause  s step  "
           "[/] speed  w weather  q quit\n";

    std::fwrite(out.data(), 1, out.size(), stdout);
    std::fflush(stdout);

    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  simInputEnd();
  return 0;
}
```

- [ ] **Step 5: Wire it into main**

In `sim/main_sim.cpp`, add `#include "app.h"` and replace:

```cpp
  // Interactive TUI arrives in Task 5.
  usage();
  return 0;
```

with:

```cpp
  return simRunApp();
```

- [ ] **Step 6: Run tests to verify they pass**

Run: `make -C sim test`
Expected: all five test binaries pass.

- [ ] **Step 7: Verify the TUI by eye**

Run: `make -C sim && ./sim/build/obegraensad-sim`
Expected: the matrix animates, `j`/`k` change effect, `r` rotates, `q` exits cleanly and the terminal is restored (no invisible cursor, no broken echo).

- [ ] **Step 8: Commit**

```bash
git add sim/tui sim/main_sim.cpp sim/tests/test_app.cpp
git commit -m "feat(sim): interactive TUI loop with keyboard control"
```

---

### Task 7: Documentation

**Files:**
- Modify: `README.md` (add a Simulator section)
- Create: `sim/README.md`

- [ ] **Step 1: Write `sim/README.md`**

```markdown
# Simulator

Renders the 16×16 LED matrix in a terminal so effects can be previewed and
developed without flashing hardware.

It compiles the *real* firmware sources — `src/screen.cpp`, `src/signs.cpp`,
`src/PluginManager.cpp` and `src/plugins/*.cpp` — against a host shim for the
Arduino API. Effects therefore behave exactly as they do on the device, including
rotation, brightness scaling and the drawing helpers.

## Build and run

    make -C sim
    ./sim/build/obegraensad-sim

Requires a C++17 compiler and a truecolor terminal. No other dependencies.

## Keys

| Key       | Action                  |
|-----------|-------------------------|
| `j` / `k` | previous / next effect  |
| `r`       | rotate 0/90/180/270     |
| `+` / `-` | global brightness       |
| `space`   | pause / resume          |
| `s`       | single-step one frame   |
| `[` / `]` | clock speed 0.25×–4×    |
| `w`       | cycle weather condition |
| `q`       | quit                    |

## Headless mode

Print frames as text, for tests or for diffing an effect's output:

    ./sim/build/obegraensad-sim --plugin Wave --frames 3 --dump

## Tests

    make -C sim test

## Developing an effect

Add the plugin to `src/plugins/`, register it in `sim/host/sim_registry.cpp`
(and in `src/main.cpp` for the firmware), then:

    make -C sim && ./sim/build/obegraensad-sim

## Limitations

- `DDPPlugin` and `ArtNetPlugin` are excluded: they render whatever an external
  controller pushes over UDP, so there is nothing of their own to preview.
- `WeatherPlugin` uses a canned forecast; `w` cycles every condition so all icons
  can be checked.
- `DrawPlugin` is driven by the web UI over websockets, so it shows a static
  buffer here.
- Serial output is suppressed so it cannot corrupt the display. Pass `--verbose`
  to send it to stderr.
```

- [ ] **Step 2: Add a Simulator section to the top-level `README.md`**

Insert after the existing feature list:

```markdown
### Simulator

Preview effects in a terminal without flashing hardware:

    make -C sim && ./sim/build/obegraensad-sim

See [`sim/README.md`](sim/README.md) for keys, headless mode and how to develop
new effects.
```

- [ ] **Step 3: Final verification**

Run:

```bash
make -C sim clean && make -C sim test && make -C sim
./sim/build/obegraensad-sim --plugin Checkerboard --frames 1 --dump
```

Expected: clean build from scratch, all tests pass, a checkerboard frame prints.

- [ ] **Step 4: Verify the firmware build is not broken**

The whole point of the `#ifdef SIMULATOR` discipline is that the device build is
untouched. If PlatformIO is available:

```bash
pio run -e esp32
```

Expected: builds as before. If PlatformIO is not installed, confirm by
inspection that every change to `src/` and `include/` is inside a `SIMULATOR`
guard:

```bash
git diff main -- src include
```

- [ ] **Step 5: Commit**

```bash
git add README.md sim/README.md
git commit -m "docs: document the TUI simulator"
```

---

## Notes for the implementer

- The spec called for CMake and a vendored ArduinoJson. Both were dropped after
  a build spike: `cmake` is not installed, and WeatherPlugin's entire JSON
  surface is two key lookups, which a 40-line stub covers. Do not reintroduce
  either.
- The spec claimed `ENABLE_SERVER`/`ENABLE_STORAGE` could be left undefined
  without touching a file. That is wrong — both are unconditionally `#define`d
  in `constants.h` and need the guards in Task 1 Step 5.
- The spec proposed making `getRotatedRenderBuffer()` public. That is not
  needed: `_render()` is a member and already calls it. Task 2 adds a public
  `render()` instead, which is a smaller change.
- `src/main.cpp` is deliberately not compiled. `currentStatus` is defined in
  `main_sim.cpp` instead.
- If a plugin turns out to need `src/config.cpp`, prefer a small
  `sim/shim/sim_config.cpp` over dragging the real config and its storage
  dependencies into the build.
