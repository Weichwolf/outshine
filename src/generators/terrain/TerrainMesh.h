#ifndef OUTSHINE_GENERATORS_TERRAIN_TERRAINMESH_H
#define OUTSHINE_GENERATORS_TERRAIN_TERRAINMESH_H

#include <cstdint>
#include <vector>

#include "GroundMesher.h"
#include "TangentFrame.h"
#include "TerrainPage.h"

namespace outshine::Generators {

struct TerrainMesh {
  std::vector<float> PositionsM;
  std::vector<uint32_t> Indices;
  double TallestM = 0.0;
  double LowestM = 0.0;
  double TallestDistanceM = 0.0;
};

[[nodiscard]] TerrainMesh BuildTerrainMesh(const Patchwork &candidate,
                                           const TangentFrame &frame,
                                           TerrainPageLayout layout,
                                           int minimumZoom = 0);

}
#endif
