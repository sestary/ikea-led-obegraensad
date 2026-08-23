# Native simulator build. No CMake, no third-party dependencies.
#
# Compiles the real firmware sources against the host shim in shim/, so effects
# behave exactly as they do on the device.

ROOT      := ..
CXX       ?= c++
# -MMD -MP emit header dependencies. Without them a change to a header does not
# rebuild the objects that include it, and a stale object with an out-of-date
# vtable crashes rather than failing to compile.
CXXFLAGS  := -std=gnu++17 -O2 -Wall -DSIMULATOR -MMD -MP \
             -I$(ROOT)/include -Ishim -Ihost -Itui -I.
BUILD     := build

FIRMWARE_SRC := $(ROOT)/src/screen.cpp \
                $(ROOT)/src/signs.cpp \
                $(ROOT)/src/PluginManager.cpp \
                $(ROOT)/src/scheduler.cpp \
                $(ROOT)/src/config.cpp \
                $(ROOT)/src/icons.cpp \
                $(ROOT)/src/scene_builder.cpp \
                $(ROOT)/src/weather_store.cpp

# DDPPlugin and ArtNet render whatever an external controller pushes over UDP,
# so they have no effect of their own to preview.
PLUGIN_SRC := $(filter-out %/DDPPlugin.cpp %/ArtNet.cpp, \
                $(wildcard $(ROOT)/src/plugins/*.cpp))

SIM_SRC := $(wildcard shim/*.cpp) $(wildcard host/*.cpp) $(wildcard tui/*.cpp)

CORE_SRC := $(FIRMWARE_SRC) $(PLUGIN_SRC) $(SIM_SRC)
CORE_OBJ := $(patsubst %.cpp,$(BUILD)/%.o,$(subst $(ROOT)/,root_/,$(CORE_SRC)))

BIN := $(BUILD)/obegraensad-sim

.PHONY: all test run clean
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
	./$(BIN)

TEST_SRC := $(wildcard tests/*.cpp)
TEST_BIN := $(patsubst tests/%.cpp,$(BUILD)/tests/%,$(TEST_SRC))

$(BUILD)/tests/%: tests/%.cpp $(CORE_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Itests $< $(CORE_OBJ) -o $@

# The pty test needs the binary: it drives the real TUI through a terminal,
# which is the only place terminal-mode bugs are reachable.
test: $(TEST_BIN) $(BIN)
	@rc=0; for t in $(TEST_BIN); do \
	  echo "== $$t"; ./$$t || rc=1; \
	done; \
	if command -v python3 >/dev/null 2>&1; then \
	  echo "== tests/test_tui_pty.py"; \
	  python3 tests/test_tui_pty.py || rc=1; \
	else \
	  echo "== tests/test_tui_pty.py (skipped: no python3)"; \
	fi; \
	exit $$rc

clean:
	rm -rf $(BUILD)

-include $(CORE_OBJ:.o=.d) $(BUILD)/main_sim.d $(TEST_BIN:=.d)
