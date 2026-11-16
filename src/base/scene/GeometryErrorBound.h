#ifndef OUTSHINE_BASE_SCENE_GEOMETRYERRORBOUND_H
#define OUTSHINE_BASE_SCENE_GEOMETRYERRORBOUND_H
#include "Box.h"
#include "ProjectedErrorBudget.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace outshine {
struct GeometryErrorBound {
  uint64_t SourceKey = 0;
  Box Bounds;
  double UpperM = 0;

  [[nodiscard]] bool Allows(uint64_t sourceKey,
                            std::optional<Vec3> eye,
                            ProjectedErrorBudget projection,
                            double translationM = 0) const noexcept {
    if (sourceKey == 0 || SourceKey != sourceKey || !eye || Bounds.Empty() ||
        !std::isfinite(UpperM) || UpperM < 0 || !std::isfinite(translationM) || translationM < 0) {
      return false;
    }
    double distanceSquared = 0;
    for (size_t axis = 0; axis < 3; ++axis) {
      if (!std::isfinite((*eye)[axis]) || !std::isfinite(Bounds.Min[axis]) ||
          !std::isfinite(Bounds.Max[axis]) || Bounds.Min[axis] > Bounds.Max[axis]) {
        return false;
      }
      const double delta =
          (*eye)[axis] - std::clamp((*eye)[axis], Bounds.Min[axis], Bounds.Max[axis]);
      distanceSquared += delta * delta;
    }
    return projection.Allows(UpperM, std::sqrt(distanceSquared) - UpperM - translationM);
  }
};
}
#endif
