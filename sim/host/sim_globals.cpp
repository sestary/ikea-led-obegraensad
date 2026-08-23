// Globals that normally live in src/main.cpp, which the simulator does not
// compile. Kept out of main_sim.cpp so the test binaries get them too.

#include "Arduino.h"
#include "PluginManager.h"
#include "constants.h"

volatile SYSTEM_STATUS currentStatus = NONE;
PluginManager pluginManager;
