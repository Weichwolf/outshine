#ifndef OUTSHINE_BASE_SPATIAL_DRAPE_H
#define OUTSHINE_BASE_SPATIAL_DRAPE_H

#include <array>
#include "Earth.h"
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
  std::function<std::optional<double>(EastNorth)> Field;

  using EastNorth = outshine::EastNorth;

  [[nodiscard]] std::optional<double> Sample(EastNorth at) const {
    if (Field) {
      const std::optional<double> field = Field(at);
      if (field) { return field; }
    }
    const std::optional<float> under = Surface.Under(
        static_cast<float>(at.EastM), static_cast<float>(RenderFrame::ZOfNorth(at.NorthM)));
    return under ? std::optional<double>(static_cast<double>(*under)) : std::nullopt;
  }

  [[nodiscard]] double At(EastNorth at, double fallback) const {
    const std::optional<double> sampled = Sample(at);
    return sampled ? *sampled : fallback;
  }
};

}
#endif
