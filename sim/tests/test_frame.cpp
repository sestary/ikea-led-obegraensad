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

static void test_rotation_90_matches_firmware_mapping() {
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
  Screen.setBrightness(MAX_BRIGHTNESS);
}

static void test_sequence_number_increments() {
  unsigned long a = simLatestFrame().seq;
  simRenderTick();
  CHECK(simLatestFrame().seq > a);
}

int main() {
  Screen.setup();
  RUN(test_pixel_reaches_frame_unrotated);
  RUN(test_rotation_90_matches_firmware_mapping);
  RUN(test_rotation_180_maps_to_opposite_corner);
  RUN(test_global_brightness_scales_frame);
  RUN(test_sequence_number_increments);
  TEST_MAIN_END;
}
