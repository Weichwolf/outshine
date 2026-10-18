#include "PreparedPlant.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_set>
#include <utility>

namespace outshine::Generators {
namespace {
bool PlacementValid(const Mat4 &transform) {
  if (!std::ranges::all_of(transform.Column, [](double v) { return std::isfinite(v); }) ||
      transform[3] != 0 || transform[7] != 0 || transform[11] != 0 || transform[15] != 1) {
    return false;
  }
  const double determinant =
      transform[0] * (transform[5] * transform[10] - transform[9] * transform[6]) -
      transform[4] * (transform[1] * transform[10] - transform[9] * transform[2]) +
      transform[8] * (transform[1] * transform[6] - transform[5] * transform[2]);
  return std::isfinite(determinant) && determinant > 0;
}
}

bool PreparedPlantIsValid(const PreparedPlant &plant) {
  if (plant.Axes.empty() || plant.Axes.front().Id != plant.Root ||
      plant.Leaves.size() > size_t{1} + std::numeric_limits<uint8_t>::max() ||
      plant.Prototypes.parts() != static_cast<int>(plant.Leaves.size() + 1) ||
      plant.Prototypes.lamps() != 0 || !plant.Prototypes.wellFormed()) {
    return false;
  }
  for (int part = 0; part < plant.Prototypes.parts(); ++part) {
    const auto &geometry = plant.Prototypes;
    if (geometry.normalsOf(part).size() != geometry.positionsOf(part).size() ||
        (part > 0 &&
         geometry.textureOf(part).size() / 2 != geometry.positionsOf(part).size() / 3) ||
        !PlacementValid(geometry.placementOf(part))) {
      return false;
    }
  }
  const size_t indices = plant.Prototypes.trianglesOf(0).size();
  if (indices > std::numeric_limits<uint32_t>::max()) { return false; }
  std::unordered_set<uint64_t> axes;
  axes.reserve(plant.Axes.size());
  size_t next = 0;
  for (const auto &axis : plant.Axes) {
    if (axis.FirstIndex != next || axis.IndexCount == 0 || axis.IndexCount % 3 != 0 ||
        axis.IndexCount > indices - next || !axes.insert(axis.Id).second) {
      return false;
    }
    next += axis.IndexCount;
  }
  if (next != indices) { return false; }
  std::unordered_set<uint64_t> leaves;
  int previous = -1;
  for (const auto &group : plant.Leaves) {
    if (std::cmp_less_equal(group.Variant, previous) || group.Ids.empty() ||
        group.Ids.size() != group.Axes.size() || group.Ids.size() != group.Transforms.size()) {
      return false;
    }
    previous = group.Variant;
    for (size_t at = 0; at < group.Ids.size(); ++at) {
      if (!axes.contains(group.Axes[at]) || !leaves.insert(group.Ids[at]).second ||
          !PlacementValid(group.Transforms[at])) {
        return false;
      }
    }
  }
  return true;
}
}
