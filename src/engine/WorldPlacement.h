#ifndef OUTSHINE_ENGINE_WORLDPLACEMENT_H
#define OUTSHINE_ENGINE_WORLDPLACEMENT_H

#include "ClusterId.h"
#include "Tile.h"

namespace outshine {

struct WorldPlacement {
  LongitudeLatitude Position;
  double AslM = 0.0;
  float YawRad = 0.0f;
  float Scale = 1.0f;

  static WorldPlacement From(const Generators::Tile &region, const Generators::Scattered &local) {
    return {.Position = region.Geo({.EastM = local.Em, .NorthM = local.Nm}),
            .AslM = local.AslM,
            .YawRad = local.YawRad,
            .Scale = local.Scale};
  }
};

} // namespace outshine
#endif
