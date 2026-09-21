#ifndef OUTSHINE_GENERATORS_TERRAIN_TERRAINPRESS_H
#define OUTSHINE_GENERATORS_TERRAIN_TERRAINPRESS_H

#include <cstddef>
#include <span>

#include "GroundMesher.h"
#include "GroundYield.h"
#include "TangentFrame.h"
#include "TerrainPage.h"

namespace outshine::Generators {

struct PressedTerrain {
  size_t Nodes = 0;
  size_t Structures = 0;
  size_t Held = 0;
  double DeepestM = 0.0;
  double RaisedM = 0.0;
  Floors Pads;
  Floors Corridors;
  double GatherMs = 0.0;
  double DecideMs = 0.0;
  double BucketMs = 0.0;
  double RejectMs = 0.0;
  double ApplyMs = 0.0;
  double WriteMs = 0.0;
  double FloorsMs = 0.0;
};

[[nodiscard]] PressedTerrain PressTerrain(std::span<const Yields> yields,
                                          Patchwork &candidate,
                                          const TangentFrame &frame,
                                          TerrainPageLayout layout,
                                          double mostEarthworkM);

}
#endif
