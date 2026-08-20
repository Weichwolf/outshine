#ifndef _FBSTATE_H
#define _FBSTATE_H
#include "AvionicsBlocks.h"

namespace outshine {

struct State {
  double NowS = 0.0;

  PlatformBlock    Platform;
  EnvironmentBlock Env;
};

}
#endif
