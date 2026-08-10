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
