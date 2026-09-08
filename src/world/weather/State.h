#ifndef OUTSHINE_WORLD_WEATHER_STATE_H
#define OUTSHINE_WORLD_WEATHER_STATE_H
#include "AvionicsBlocks.h"

namespace outshine {

struct State {
  double NowS = 0.0;

  PlatformBlock Platform;
  EnvironmentBlock Env;
};

}
#endif
