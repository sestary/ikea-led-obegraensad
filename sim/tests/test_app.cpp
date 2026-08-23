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
  CHECK_EQ(s.selected, 0); // already at the top
  simAppHandleKey(s, 'j');
  CHECK_EQ(s.selected, 1);
  for (int i = 0; i < 100; i++)
    simAppHandleKey(s, 'j');
  CHECK_EQ(s.selected, (int)pluginManager.getNumPlugins() - 1);
}

static void test_space_toggles_pause_and_q_quits() {
  SimAppState s = freshState();
  simAppHandleKey(s, ' ');
  CHECK(s.paused);
  CHECK(simClockIsPaused());
  simAppHandleKey(s, ' ');
  CHECK(!s.paused);
  CHECK(!simClockIsPaused());
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
  simClockSetSpeed(1.0);
}

static void test_step_advances_clock_while_paused() {
  SimAppState s = freshState();
  simAppHandleKey(s, ' ');
  unsigned long a = millis();
  simAppHandleKey(s, 's');
  CHECK(millis() > a);
  simAppHandleKey(s, ' ');
}

int main() {
  Screen.setup();
  RUN(test_r_cycles_rotation_through_four_values);
  RUN(test_brightness_clamps_at_both_ends);
  RUN(test_jk_moves_selection_and_clamps);
  RUN(test_space_toggles_pause_and_q_quits);
  RUN(test_brackets_change_clock_speed);
  RUN(test_step_advances_clock_while_paused);
  TEST_MAIN_END;
}
