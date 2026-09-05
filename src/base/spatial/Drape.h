#ifndef OUTSHINE_BASE_SPATIAL_DRAPE_H
#define OUTSHINE_BASE_SPATIAL_DRAPE_H

#include <array>
#include "math/RenderFrame.h"
#include <functional>
#include <optional>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <span>
#include <vector>
#include <limits>

#include "TriangleBvh.h"
#include "math/Units.h"

namespace outshine {

inline constexpr size_t kDrapeRungs = 6;
inline constexpr double kFinestCellM = 32.0;
inline constexpr double kCellPerRung = 8.0;
inline constexpr double kDrapeGridM = 32.0;

[[nodiscard]] inline double DrapeCellM(size_t rung) {
  double cellM = kFinestCellM;
  for (size_t step = 0; step < rung; ++step) { cellM *= kCellPerRung; }
  return cellM;
}

struct Drape {
  const TriangleBvh &Surface;
  std::function<std::optional<double>(double, double)> Field;

  struct EastNorth {
    double EastM = 0.0;
    double NorthM = 0.0;
  };

  [[nodiscard]] double At(EastNorth at, double fallback) const {
    if (Field) {
      const std::optional<double> field = Field(at.EastM, RenderFrame::ZOfNorth(at.NorthM));
      return field ? *field : fallback;
    }
    const std::optional<float> under = Surface.Under(
        static_cast<float>(at.EastM), static_cast<float>(RenderFrame::ZOfNorth(at.NorthM)));
    return under ? static_cast<double>(*under) : fallback;
  }
};

} // namespace outshine
#endif
