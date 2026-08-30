#ifndef OUTSHINE_WORLD_WEATHER_AVIONICSBLOCKS_H
#define OUTSHINE_WORLD_WEATHER_AVIONICSBLOCKS_H

#include "BlockStatus.h"
#include <cstdint>

namespace outshine {

struct PlatformBlock {
  BlockHeader H;
  float RollDeg = 0.0f, PitchDeg = 0.0f, YawDeg = 0.0f;
  float AltM = 0.0f;
  float EastM = 0.0f, NorthM = 0.0f;
  float GsMs = 0.0f, TasMs = 0.0f, VsMs = 0.0f;
  float HomeDistM = 0.0f, HomeBearingDeg = 0.0f;
};

struct EnvironmentBlock {
  BlockHeader H;
  float CloudCover = 0.0f;
  float CloudLow = 0.0f, CloudMid = 0.0f, CloudHigh = 0.0f;
  float CloudBaseAglM = 0.0f;
  float SunElDeg = 0.0f, SunAzDeg = 0.0f;
  float MoonElDeg = 0.0f, MoonAzDeg = 0.0f, MoonPhase = 0.0f;
};

} // namespace outshine
#endif
