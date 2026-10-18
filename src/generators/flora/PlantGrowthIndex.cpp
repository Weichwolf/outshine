#include "PlantGrowthIndex.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>

namespace outshine::Generators {
namespace {
double SquaredDistance(PlantGrowthPoint first, PlantGrowthPoint second) {
  double value = 0;
  for (size_t axis = 0; axis < 3; ++axis) {
    const double difference = first[axis] - second[axis];
    value += difference * difference;
  }
  return value;
}
}

void PlantGrowthIndex::Build(std::span<const PlantGrowthPoint> points, PlantGrowthStats &stats) {
  Points_ = points;
  const auto order = std::span(Order_).first(points.size());
  std::ranges::iota(order, uint32_t{0});
  Partition(order, 0, stats);
}

void PlantGrowthIndex::Partition(std::span<uint32_t> order, size_t axis, PlantGrowthStats &stats) {
  if (order.empty()) { return; }
  const size_t middle = order.size() / 2;
  std::ranges::nth_element(order,
                           order.begin() + static_cast<ptrdiff_t>(middle),
                           [this, axis, &stats](uint32_t first, uint32_t second) {
                             ++stats.PartitionComparisons;
                             if (Points_[first][axis] == Points_[second][axis]) {
                               return first < second;
                             }
                             return Points_[first][axis] < Points_[second][axis];
                           });
  Partition(order.first(middle), (axis + 1) % 3, stats);
  Partition(order.subspan(middle + 1), (axis + 1) % 3, stats);
}

PlantGrowthIndex::Hit
PlantGrowthIndex::Nearest(PlantGrowthPoint point, double radiusM, PlantGrowthStats &stats) const {
  ++stats.NearestQueries;
  Hit hit{.SquaredM = radiusM * radiusM};
  Search(std::span(Order_).first(Points_.size()), 0, point, hit, stats);
  return hit;
}

void PlantGrowthIndex::Search(std::span<const uint32_t> order,
                              size_t axis,
                              PlantGrowthPoint point,
                              Hit &hit,
                              PlantGrowthStats &stats) const {
  if (order.empty()) { return; }
  const size_t middle = order.size() / 2;
  const uint32_t node = order[middle];
  ++stats.DistanceChecks;
  const double distance = SquaredDistance(point, Points_[node]);
  if (distance < hit.SquaredM || (distance == hit.SquaredM && node < hit.Node)) {
    hit = {.Node = node, .SquaredM = distance};
  }
  const double delta = point[axis] - Points_[node][axis];
  const auto left = order.first(middle);
  const auto right = order.subspan(middle + 1);
  Search(delta <= 0 ? left : right, (axis + 1) % 3, point, hit, stats);
  if (delta * delta <= hit.SquaredM) {
    Search(delta <= 0 ? right : left, (axis + 1) % 3, point, hit, stats);
  }
}

void PlantGrowthIndex::DiscardFollowing(uint32_t first,
                                        double radiusM,
                                        std::span<uint8_t> free,
                                        PlantGrowthStats &stats) const {
  ++stats.RadiusQueries;
  Discard(std::span(Order_).first(Points_.size()), 0, first, radiusM * radiusM, free, stats);
}

void PlantGrowthIndex::Discard(std::span<const uint32_t> order,
                               size_t axis,
                               uint32_t first,
                               double squaredM,
                               std::span<uint8_t> free,
                               PlantGrowthStats &stats) const {
  if (order.empty()) { return; }
  const size_t middle = order.size() / 2;
  const uint32_t node = order[middle];
  ++stats.DistanceChecks;
  if (node > first && SquaredDistance(Points_[first], Points_[node]) <= squaredM) {
    free[node] = 0;
  }
  const double delta = Points_[first][axis] - Points_[node][axis];
  const auto left = order.first(middle);
  const auto right = order.subspan(middle + 1);
  Discard(delta <= 0 ? left : right, (axis + 1) % 3, first, squaredM, free, stats);
  if (delta * delta <= squaredM) {
    Discard(delta <= 0 ? right : left, (axis + 1) % 3, first, squaredM, free, stats);
  }
}
}
