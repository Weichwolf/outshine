#ifndef OUTSHINE_ENGINE_STREAMING_TERRAINSOURCECOVERAGE_H
#define OUTSHINE_ENGINE_STREAMING_TERRAINSOURCECOVERAGE_H

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include <world/data/Address.h>

namespace outshine::Generators::Osm {
class OsmField;
}

namespace outshine {

struct TerrainSourceCoverage {
  static constexpr size_t MaximumFields = 8192;
  int FinestZoom = 0;
  int GroundZoom = -1;
  const ::outshine::Generators::Osm::OsmField *Vectors = nullptr;
  bool BuildingFootprints = true;
  std::span<const Data::TileId> AdditionalTiles;
  size_t MaximumTiles = MaximumFields;
};

[[nodiscard]] std::expected<std::vector<Data::TileId>, std::string>
PlanTerrainSourceTiles(std::span<const Data::TileId> sources, TerrainSourceCoverage coverage);
}

#endif
