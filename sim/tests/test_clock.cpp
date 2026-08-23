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

static void test_step_is_in_simulated_ms_regardless_of_speed() {
  simClockSetPaused(true);
  simClockSetSpeed(1.0);
  unsigned long a = millis();
  simClockStep(100);
  CHECK_EQ(millis(), a + 100);
  // Speed scales real elapsed time only; an explicit step must not be scaled.
  simClockSetSpeed(2.0);
  simClockStep(100);
  CHECK_EQ(millis(), a + 200);
}

static void test_speed_roundtrips_and_clamps() {
  simClockSetSpeed(0.25);
  CHECK(simClockGetSpeed() == 0.25);
  simClockSetSpeed(4.0);
  CHECK(simClockGetSpeed() == 4.0);
  simClockSetSpeed(100.0);
  CHECK(simClockGetSpeed() == 4.0);
  simClockSetSpeed(0.001);
  CHECK(simClockGetSpeed() == 0.25);
  simClockSetSpeed(1.0);
}

int main() {
  RUN(test_paused_clock_does_not_advance);
  RUN(test_step_advances_exactly);
  RUN(test_step_is_in_simulated_ms_regardless_of_speed);
  RUN(test_speed_roundtrips_and_clamps);
  TEST_MAIN_END;
}
