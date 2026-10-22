#include "UrbanAreaInstances.h"
#include "UrbanInstancePreparation.h"
#include <cstddef>
#include <expected>
#include <vector>

namespace outshine {
std::expected<std::vector<Mat4>, UrbanInstanceError>
PrepareUrbanPatchInstances(const Generators::Osm::UrbanArea &area,
                           Generators::Osm::UrbanPatchRange patch,
                           UrbanPatchHeights groundAslM,
                           const TangentFrame &frame,
                           size_t instancesMost) {
  const auto occupied = UrbanInstanceDetail::CountInstances(area, patch, instancesMost);
  if (!occupied) { return std::unexpected(occupied.error()); }
  if (*occupied == 0) { return std::vector<Mat4>{}; }
  if (groundAslM.data_handle() == nullptr || groundAslM.extent(0) != patch.Rows ||
      groundAslM.extent(1) != patch.Columns) {
    return std::unexpected(UrbanInstanceError::MissingTerrain);
  }
  return UrbanInstanceDetail::PrepareInstances(
      area, patch, frame, *occupied, [&](size_t row, size_t column, auto) {
        return groundAslM[row, column];
      });
}
}
