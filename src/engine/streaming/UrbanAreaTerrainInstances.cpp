#include "UrbanAreaTerrainInstances.h"
#include "UrbanInstancePreparation.h"
#include "TerrainResidency.h"
#include <cstddef>
#include <expected>
#include <vector>

namespace outshine {

std::expected<std::vector<Mat4>, UrbanInstanceError>
PrepareUrbanPatchInstances(const Generators::Osm::UrbanArea &area,
                           Generators::Osm::UrbanPatchRange patch,
                           const TerrainResidency &terrain,
                           const TangentFrame &frame,
                           size_t instancesMost) {
  const auto occupied = UrbanInstanceDetail::CountInstances(area, patch, instancesMost);
  if (!occupied) { return std::unexpected(occupied.error()); }
  if (*occupied == 0) { return std::vector<Mat4>{}; }
  if (!terrain.HasPublishedSurface()) {
    return std::unexpected(UrbanInstanceError::MissingTerrain);
  }
  return UrbanInstanceDetail::PrepareInstances(
      area, patch, frame, *occupied, [&](size_t, size_t, auto position) {
        return terrain.HeightMAt(position);
      });
}
}
