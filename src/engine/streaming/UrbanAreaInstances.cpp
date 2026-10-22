#include "UrbanAreaInstances.h"
#include "WorldPlacement.h"
#include "math/Units.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <vector>

namespace outshine {
namespace {
Mat4 LotModel(const Generators::Osm::UrbanLot &lot,
              const WorldPlacement &placement,
              const TangentFrame &frame) {
  Mat4 model = placement.ModelIn(frame);
  const std::array<double, 3> scale{lot.EastSpanM, lot.HeightM, lot.NorthSpanM};
  for (size_t axis = 0; axis < scale.size(); ++axis) {
    for (size_t component = 0; component < 3; ++component) {
      model.At(component, axis) *= scale[axis];
    }
  }
  return model;
}
}

std::expected<std::vector<Mat4>, UrbanInstanceError>
PrepareUrbanPatchInstances(const Generators::Osm::UrbanArea &area,
                           Generators::Osm::UrbanPatchRange patch,
                           UrbanPatchHeights groundAslM,
                           const TangentFrame &frame,
                           size_t instancesMost) {
  if (patch.Columns == 0 || patch.Rows == 0 || patch.Column >= area.Columns ||
      patch.Row >= area.Rows || patch.Columns > area.Columns - patch.Column ||
      patch.Rows > area.Rows - patch.Row || patch.Columns > area.Recipe.PatchSide ||
      patch.Rows > area.Recipe.PatchSide ||
      area.Tiers.size() != static_cast<size_t>(area.Columns) * area.Rows) {
    return std::unexpected(UrbanInstanceError::InvalidRequest);
  }
  size_t occupied = 0;
  for (uint32_t row = patch.Row; row < patch.Row + patch.Rows; ++row) {
    for (uint32_t column = patch.Column; column < patch.Column + patch.Columns; ++column) {
      occupied += area.Tiers[static_cast<size_t>(row) * area.Columns + column] != 0 ? 1u : 0u;
      if (occupied > instancesMost) { return std::unexpected(UrbanInstanceError::OverBudget); }
    }
  }
  if (occupied == 0) { return std::vector<Mat4>{}; }
  if (groundAslM.data_handle() == nullptr || groundAslM.extent(0) != patch.Rows ||
      groundAslM.extent(1) != patch.Columns) {
    return std::unexpected(UrbanInstanceError::MissingTerrain);
  }
  std::vector<Mat4> models;
  models.reserve(occupied);
  for (uint32_t row = patch.Row; row < patch.Row + patch.Rows; ++row) {
    for (uint32_t column = patch.Column; column < patch.Column + patch.Columns; ++column) {
      const auto lot = area.LotAt(column, row);
      if (!lot) { continue; }
      const LongitudeLatitude position{.LongitudeDeg = Wrap180(lot->Centre.LongitudeDeg),
                                       .LatitudeDeg = lot->Centre.LatitudeDeg};
      const auto height = groundAslM[row - patch.Row, column - patch.Column];
      if (!height || !std::isfinite(*height)) {
        return std::unexpected(UrbanInstanceError::MissingTerrain);
      }
      models.push_back(LotModel(*lot, {.Position = position, .AslM = *height}, frame));
    }
  }
  return models;
}
}
