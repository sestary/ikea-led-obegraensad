#include "input.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <termios.h>
#include <unistd.h>

namespace {
termios g_saved{};
bool g_active = false;

void onSignal(int sig) {
  simInputEnd();
  std::signal(sig, SIG_DFL);
  std::raise(sig);
}
} // namespace

void simInputBegin() {
  if (g_active)
    return;
  if (tcgetattr(STDIN_FILENO, &g_saved) != 0)
    return; // not a tty; leave stdin alone

  // VMIN=0 with VTIME=0 makes read() return immediately with 0 bytes when no
  // key is pending, which is all simInputPoll() needs.
  //
  // Do NOT add O_NONBLOCK here. On a terminal stdin and stdout are the same
  // open file description, so setting it on stdin also sets it on stdout, and
  // a full 4 KB frame write then returns after ~1 KB with the rest silently
  // dropped — the panel renders only its first couple of rows.
  termios raw = g_saved;
  raw.c_lflag &= ~(ICANON | ECHO);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

  std::printf("\033[?1049h"); // alternate screen
  std::printf("\033[?25l");   // hide cursor
  std::fflush(stdout);
  g_active = true;

  // Ctrl-C and friends must not leave the terminal raw and cursorless.
  std::atexit(simInputEnd);
  std::signal(SIGINT, onSignal);
  std::signal(SIGTERM, onSignal);
}

void simInputEnd() {
  if (!g_active)
    return;
  g_active = false;

  std::printf("\033[0m");     // reset colors
  std::printf("\033[?25h");   // show cursor
  std::printf("\033[?1049l"); // leave alternate screen
  std::fflush(stdout);

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_saved);
}

char simInputPoll() {
  char c = 0;
  ssize_t n = read(STDIN_FILENO, &c, 1);
  return n == 1 ? c : 0;
}
