#ifndef OUTSHINE_BASE_SPATIAL_TRIANGLEREGION_H
#define OUTSHINE_BASE_SPATIAL_TRIANGLEREGION_H
#include <array>
#include <optional>
#include "math/Vec3.h"

namespace outshine {
struct PointEnclosure {
  Vec3 EstimateM;
  double RadiusM = 0;
};

struct TriangleRegionEnclosure {
  PointEnclosure InteriorSample;
  double RadiusM = 0;
};

[[nodiscard]] std::optional<PointEnclosure>
EnclosePointBlend(const std::array<PointEnclosure, 2> &endpoints, double towardEnd) noexcept;
[[nodiscard]] std::optional<TriangleRegionEnclosure>
EncloseTriangleRegion(const std::array<PointEnclosure, 3> &vertices) noexcept;
}
#endif
