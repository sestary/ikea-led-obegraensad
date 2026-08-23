#include "sim_registry.h"

#include "PluginManager.h"

#include "plugins/AnimationPlugin.h"
#include "plugins/BigClockPlugin.h"
#include "plugins/Blob.h"
#include "plugins/BreakoutPlugin.h"
#include "plugins/BubblesPlugin.h"
#include "plugins/CheckerboardPlugin.h"
#include "plugins/CirclePlugin.h"
#include "plugins/ClockPlugin.h"
#include "plugins/CometPlugin.h"
#include "plugins/DrawPlugin.h"
#include "plugins/FirefliesPlugin.h"
#include "plugins/FireworkPlugin.h"
#include "plugins/GameOfLifePlugin.h"
#include "plugins/LinesPlugin.h"
#include "plugins/MatrixClockPlugin.h"
#include "plugins/MatrixRainPlugin.h"
#include "plugins/MeteorShowerPlugin.h"
#include "plugins/PongClockPlugin.h"
#include "plugins/RadarPlugin.h"
#include "plugins/RainPlugin.h"
#include "plugins/ScanlinesPlugin.h"
#include "plugins/SnakePlugin.h"
#include "plugins/SparkleFieldPlugin.h"
#include "plugins/SpiralPlugin.h"
#include "plugins/StarsPlugin.h"
#include "plugins/TickingClockPlugin.h"
#include "plugins/WaveBarsPlugin.h"
#include "plugins/WavePlugin.h"
#include "plugins/WeatherPlugin.h"

void simRegisterPlugins() {
  if (pluginManager.getNumPlugins() > 0)
    return;

  pluginManager.addPlugin(new DrawPlugin());
  pluginManager.addPlugin(new BreakoutPlugin());
  pluginManager.addPlugin(new SnakePlugin());
  pluginManager.addPlugin(new GameOfLifePlugin());
  pluginManager.addPlugin(new StarsPlugin());
  pluginManager.addPlugin(new LinesPlugin());
  pluginManager.addPlugin(new CirclePlugin());
  pluginManager.addPlugin(new RainPlugin());
  pluginManager.addPlugin(new MatrixRainPlugin());
  pluginManager.addPlugin(new FireworkPlugin());
  pluginManager.addPlugin(new BlobPlugin());
  pluginManager.addPlugin(new SpiralPlugin());
  pluginManager.addPlugin(new WavePlugin());
  pluginManager.addPlugin(new CheckerboardPlugin());
  pluginManager.addPlugin(new RadarPlugin());
  pluginManager.addPlugin(new BubblesPlugin());
  pluginManager.addPlugin(new CometPlugin());
  pluginManager.addPlugin(new FirefliesPlugin());
  pluginManager.addPlugin(new MeteorShowerPlugin());
  pluginManager.addPlugin(new ScanlinesPlugin());
  pluginManager.addPlugin(new SparkleFieldPlugin());
  pluginManager.addPlugin(new WaveBarsPlugin());
  pluginManager.addPlugin(new BigClockPlugin());
  pluginManager.addPlugin(new ClockPlugin());
  pluginManager.addPlugin(new PongClockPlugin());
  pluginManager.addPlugin(new TickingClockPlugin());
  pluginManager.addPlugin(new WeatherPlugin());
  pluginManager.addPlugin(new MatrixClockPlugin());
  pluginManager.addPlugin(new AnimationPlugin());
}
