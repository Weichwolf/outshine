#ifndef OUTSHINE_GENERATORS_TERRAIN_TERRAINSEAMS_H
#define OUTSHINE_GENERATORS_TERRAIN_TERRAINSEAMS_H

#include "TerrainPatchwork.h"
#include <cstddef>
#include <expected>
#include <string>

namespace outshine::Generators::Terrain {

struct TerrainSeamStats {
  double EvenM = 0.0;
  double OddBeforeM = 0.0;
  double OddAfterM = 0.0;
  double WorstLongitudeDeg = 0.0;
  double WorstLatitudeDeg = 0.0;
  int WorstFineZoom = 0;
  int WorstCoarseZoom = 0;
  size_t Edges = 0;
};

struct TerrainSeamMetrics {
  TerrainSeamStats Virtual;
  TerrainSeamStats Real;
};

[[nodiscard]] std::expected<TerrainSeamMetrics, std::string> StitchTerrainSeams(Patchwork &laid);

}
#endif
