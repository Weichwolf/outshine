#ifndef OUTSHINE_ENGINE_STREAMING_TERRAINPATHPREPARATION_H
#define OUTSHINE_ENGINE_STREAMING_TERRAINPATHPREPARATION_H

#include <chrono>
#include <expected>
#include <span>
#include <string>
#include <vector>
#include "GroundMesher.h"
#include "SurfacePreparation.h"

namespace outshine {

struct TerrainPathPlan {
  static constexpr size_t MaximumPoints = 216001;
  static constexpr size_t MaximumTiles = 8192;
  std::vector<Data::TileId> Fields;
  std::vector<Data::TileId> Vectors;
};

struct TerrainPathSources {
  int VectorZoom = -1;
  std::span<const Ground::ClassField::SourceWindow> Classification;
  std::span<const Data::TileId> RoadTiles;
};

[[nodiscard]] std::expected<TerrainPathPlan, std::string> PlanTerrainPath(
    std::span<const Around> path, const Ground::GroundStream &ground, TerrainPathSources sources);

[[nodiscard]] std::expected<void, std::string>
PrepareTerrainPath(TerrainPathPlan plan,
                   const Ground::SurfacePreparation &stack,
                   std::chrono::steady_clock::time_point deadline);
}
#endif
