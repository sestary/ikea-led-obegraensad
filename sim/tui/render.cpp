#include "render.h"

#include "PluginManager.h"
#include "constants.h"
#include <cstdio>

namespace {
// The panel is single-color warm white, brightness only. Map onto a warm ramp
// so the output reads like the lamp rather than a grey monitor.
void warmWhite(uint8_t v, int &r, int &g, int &b) {
  r = v;
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
  char buf[192];
  std::snprintf(buf, sizeof(buf),
                "%-14s rot %3d°   bri %3d   %4.0ffps   %.2f×", plugin,
                rotation, static_cast<int>(brightness), fps, speed);
  return std::string(buf);
}

std::string simRenderSidebar(int selected, int firstVisible, int visibleRows) {
  std::string out;
  auto &plugins = pluginManager.getAllPlugins();
  for (int i = 0; i < visibleRows; i++) {
    int idx = firstVisible + i;
    if (idx >= static_cast<int>(plugins.size()))
      break;
    out += (idx == selected) ? "> " : "  ";
    out += plugins[idx]->getName();
    out += "\n";
  }
  return out;
}
