#include "test_main.h"

#include "Arduino.h"
#include "constants.h"
#include "frame.h"
#include "render.h"
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
  CHECK(simRenderMatrix(f).find("▀") != std::string::npos);
}

static void test_matrix_emits_truecolor_escapes() {
  SimFrame f{};
  f.px[0] = 255;
  std::string out = simRenderMatrix(f);
  CHECK(out.find("\033[38;2;") != std::string::npos);
  CHECK(out.find("\033[48;2;") != std::string::npos);
}

static void test_all_off_frame_still_renders_full_grid() {
  SimFrame f{};
  CHECK_EQ(countLines(simRenderMatrix(f)), ROWS / 2);
}

static void test_brightness_reaches_the_escape_sequence() {
  SimFrame f{};
  f.px[0] = 255;
  // Warm white at full brightness: red channel is 255.
  CHECK(simRenderMatrix(f).find("\033[38;2;255;") != std::string::npos);
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
  RUN(test_all_off_frame_still_renders_full_grid);
  RUN(test_brightness_reaches_the_escape_sequence);
  RUN(test_status_line_reports_state);
  TEST_MAIN_END;
}
