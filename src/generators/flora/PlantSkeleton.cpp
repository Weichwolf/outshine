#include "PlantSkeleton.h"
#include "PlantGrowth.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>

namespace outshine::Generators {
namespace {
constexpr double kMaximumCoordinateM = 100000, kMinimumSeedEdgeM = 1e-8;
}

bool ValidPlantGrowthPoint(PlantGrowthPoint point) noexcept {
  return std::ranges::all_of(point, [](double value) {
    return std::isfinite(value) && std::abs(value) <= kMaximumCoordinateM;
  });
}

bool ValidPlantSkeleton(std::span<const PlantGrowthPoint> positions,
                        std::span<const int32_t> parents) noexcept {
  if (positions.size() < 2 ||
      positions.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max()) ||
      positions.size() != parents.size() || parents[0] != -1 ||
      !std::ranges::all_of(positions, ValidPlantGrowthPoint)) {
    return false;
  }
  for (size_t child = 1; child < parents.size(); ++child) {
    const int32_t parent = parents[child];
    if (parent < 0 || std::cmp_greater_equal(parent, child)) { return false; }
    double squared = 0;
    for (size_t axis = 0; axis < 3; ++axis) {
      const double difference =
          positions[child][axis] - positions[static_cast<size_t>(parent)][axis];
      squared += difference * difference;
    }
    if (std::sqrt(squared) <= kMinimumSeedEdgeM) { return false; }
  }
  return true;
}
}
