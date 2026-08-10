#include "Arduino.h"
#include "app.h"
#include "constants.h"
#include "dump.h"
#include "screen.h"
#include <cstdio>
#include <cstdlib>
#include <string>

static void usage() {
  std::printf("usage: obegraensad-sim [options]\n"
              "  --plugin NAME   effect to run headlessly\n"
              "  --frames N      frames to emit in --dump mode (default 1)\n"
              "  --dump          print frames as text instead of running the TUI\n"
              "  --rotate N      panel orientation: 0=0° 1=90° 2=180° 3=270°\n"
              "  --verbose       send firmware Serial output to stderr\n"
              "  --help          this message\n"
              "\nWith no options, starts the interactive TUI.\n");
}

int main(int argc, char **argv) {
  std::string plugin;
  int frames = 1;
  bool dump = false;
  int rotation = 0;

  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--plugin" && i + 1 < argc) {
      plugin = argv[++i];
    } else if (a == "--frames" && i + 1 < argc) {
      frames = std::atoi(argv[++i]);
    } else if (a == "--rotate" && i + 1 < argc) {
      rotation = std::atoi(argv[++i]) & 0x3;
    } else if (a == "--dump") {
      dump = true;
    } else if (a == "--verbose") {
      simSerialSetVerbose(true);
    } else if (a == "--help") {
      usage();
      return 0;
    } else {
      std::fprintf(stderr, "unknown argument: %s\n", a.c_str());
      usage();
      return 2;
    }
  }

  Screen.setup();

  if (dump) {
    if (plugin.empty()) {
      std::fprintf(stderr, "--dump requires --plugin NAME\n");
      return 2;
    }
    return simRunHeadless(plugin, frames, rotation);
  }

  return simRunApp();
}
