#include "PreparedBuildingAssets.h"
#include "Geodesy.h"
#include "math/Units.h"
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace outshine::Generators {

Box PreparedBuildingAssets::StructureBounds(const PreparedStructureTile &base, size_t index) {
  Box bounds = base.Surfaces[index].Bounds();
  if (!bounds.Empty()) {
    for (size_t axis = 0; axis < 3; ++axis) {
      bounds.Min[axis] += base.AnchorEcef[axis];
      bounds.Max[axis] += base.AnchorEcef[axis];
    }
    return bounds;
  }
  const auto &structure = base.Structures[index];
  const auto &region = structure.Bounds;
  const double bottom = std::min(structure.Standing.FootM, structure.Standing.BaseM);
  const double top = structure.Standing.SeatM + structure.Standing.HeightM;
  Vec3 centre;
  GeoToEcef({.LongitudeDeg = (region.MinLonDeg + region.MaxLonDeg) * 0.5,
             .LatitudeDeg = (region.MinLatDeg + region.MaxLatDeg) * 0.5,
             .HeightM = (bottom + top) * 0.5},
            centre);
  constexpr double earthDerivativeBoundM = 6400000.0;
  const double radius =
      (earthDerivativeBoundM + std::max(std::abs(bottom), std::abs(top))) *
          (region.MaxLonDeg - region.MinLonDeg + region.MaxLatDeg - region.MinLatDeg) * kDeg2Rad *
          0.5 +
      std::abs(top - bottom) * 0.5;
  for (size_t axis = 0; axis < 3; ++axis) {
    bounds.Min[axis] = centre[axis] - radius;
    bounds.Max[axis] = centre[axis] + radius;
  }
  return bounds;
}

Box PreparedBuildingAssets::Bounds(const PreparedStructureTile &base) {
  Box bounds;
  for (size_t index = 0; index < base.Structures.size(); ++index) {
    bounds.Cover(StructureBounds(base, index));
  }
  if (bounds.Empty()) { bounds.Cover(base.AnchorEcef); }
  return bounds;
}

}
