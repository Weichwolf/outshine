#include "PlantGrowthIndex.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace {
outshine::Generators::PlantGrowthIndex::Hit
BruteNearest(std::span<const outshine::Generators::PlantGrowthPoint> points,
             outshine::Generators::PlantGrowthPoint query,
             double radiusM) {
  uint32_t expected = std::numeric_limits<uint32_t>::max();
  double squared = radiusM * radiusM;
  for (size_t row = 0; row < points.size(); ++row) {
    double candidate = 0;
    for (size_t axis = 0; axis < 3; ++axis) {
      const double delta = query[axis] - points[row][axis];
      candidate += delta * delta;
    }
    if (candidate < squared || (candidate == squared && row < expected)) {
      expected = static_cast<uint32_t>(row);
      squared = candidate;
    }
  }
  return {.Node = expected, .SquaredM = squared};
}
}

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr std::array<PlantGrowthPoint, 5> kPoints{
      PlantGrowthPoint{-1, 0, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
  PlantGrowthIndex index(kPoints.size());
  PlantGrowthStats stats;
  index.Build(kPoints, stats);
  for (int x = -12; x <= 12; ++x) {
    for (int y = -12; y <= 12; ++y) {
      for (const double radius : {0.01, 0.5, 1.0, 2.0, 8.0}) {
        const PlantGrowthPoint query{static_cast<double>(x) / 4, static_cast<double>(y) / 4, 0};
        const auto expected = BruteNearest(kPoints, query, radius);
        const auto actual = index.Nearest(query, radius, stats);
        CHECK(actual.Node == expected.Node && actual.SquaredM == expected.SquaredM,
              "median index matches complete search including exact ties and radius boundaries");
      }
    }
  }
  constexpr std::array<PlantGrowthPoint, 3> kChain{
      PlantGrowthPoint{0, 0, 0}, {0.75, 0, 0}, {1.5, 0, 0}};
  index.Build(kChain, stats);
  std::array<uint8_t, 3> free{1, 1, 1};
  for (uint32_t row = 0; row < free.size(); ++row) {
    if (free[row] != 0) { index.DiscardFollowing(row, 1, free, stats); }
  }
  CHECK((free == std::array<uint8_t, 3>{1, 0, 1}),
        "earliest free proposal wins without incorrectly merging a transitive proximity chain");
  index.Build({}, stats);
  CHECK(!index.Nearest({}, 1, stats).Found(), "an empty borrowed point view has no hit");
  return Report();
}
