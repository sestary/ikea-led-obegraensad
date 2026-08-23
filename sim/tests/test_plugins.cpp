#include "test_main.h"

#include "Arduino.h"
#include "PluginManager.h"
#include "frame.h"
#include "screen.h"
#include "sim_clock.h"
#include "sim_registry.h"

// Every registered plugin must survive setup plus a run of frames without
// crashing or writing outside the buffer.
static void test_all_plugins_run_without_crashing() {
  simRegisterPlugins();
  CHECK_EQ((int)pluginManager.getNumPlugins(), 29);
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

static void test_every_plugin_has_a_name() {
  simRegisterPlugins();
  for (Plugin *p : pluginManager.getAllPlugins()) {
    CHECK(p->getName() != nullptr);
    CHECK(p->getName()[0] != '\0');
  }
}

int main() {
  Screen.setup();
  RUN(test_all_plugins_run_without_crashing);
  RUN(test_every_plugin_has_a_name);
  TEST_MAIN_END;
}
