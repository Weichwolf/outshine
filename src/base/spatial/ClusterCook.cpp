#include "ClusterCook.h"
#include "math/Vec3.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace outshine {
namespace Says {
constexpr std::array kClusterErrors = {
    "cluster input has an incomplete vertex or triangle layout",
    "cluster input contains an out-of-range vertex index",
    "cluster input contains a non-finite position",
    "cluster triangle limit is outside the supported index range",
    "cluster input exceeds the 32-bit index range",
    "cluster bounds cannot be represented as a finite GPU sphere"};
}

std::string_view Describe(ClusterError error) noexcept {
  const auto index = static_cast<size_t>(error);
  return index < Says::kClusterErrors.size() ? Says::kClusterErrors[index] : std::string_view{};
}

namespace {
constexpr uint32_t kMortonBits = 0x3ffu;
constexpr uint32_t kMortonSpread16 = 0x030000ffu;
constexpr uint32_t kMortonSpread8 = 0x0300f00fu;
constexpr uint32_t kMortonSpread4 = 0x030c30c3u;
constexpr uint32_t kMortonSpread2 = 0x09249249u;
constexpr double kMortonSteps = 1023.0;
constexpr size_t kTriangleCorners = 3;

struct Bounds {
  Vec3 Low = {{std::numeric_limits<double>::max(),
               std::numeric_limits<double>::max(),
               std::numeric_limits<double>::max()}};
  Vec3 High = {{std::numeric_limits<double>::lowest(),
                std::numeric_limits<double>::lowest(),
                std::numeric_limits<double>::lowest()}};

  void Include(const Vec3 &point) {
    for (size_t axis = 0; axis < 3; ++axis) {
      Low[axis] = std::min(Low[axis], point[axis]);
      High[axis] = std::max(High[axis], point[axis]);
    }
  }
};

Vec3 Position(ClusterMeshInput input, size_t index) {
  const size_t first = index * input.StrideFloats;
  return {{input.PositionsM[first], input.PositionsM[first + 1], input.PositionsM[first + 2]}};
}

std::expected<Bounds, ClusterError> Validate(ClusterMeshInput input, uint32_t triangleLimit) {
  constexpr size_t maximum = std::numeric_limits<uint32_t>::max();
  if (triangleLimit == 0 || triangleLimit > maximum / kTriangleCorners) {
    return std::unexpected(ClusterError::InvalidLimit);
  }
  if (input.StrideFloats < 3 || input.PositionsM.size() % input.StrideFloats != 0 ||
      input.Indices.size() % kTriangleCorners != 0) {
    return std::unexpected(ClusterError::InvalidLayout);
  }
  const size_t vertices = input.PositionsM.size() / input.StrideFloats;
  if (vertices > maximum || input.Indices.size() > maximum) {
    return std::unexpected(ClusterError::CapacityExceeded);
  }
  Bounds bounds;
  for (size_t vertex = 0; vertex < vertices; ++vertex) {
    const Vec3 point = Position(input, vertex);
    if (!std::isfinite(point[0]) || !std::isfinite(point[1]) || !std::isfinite(point[2])) {
      return std::unexpected(ClusterError::NonFinitePosition);
    }
    bounds.Include(point);
  }
  for (const uint32_t index : input.Indices) {
    if (index >= vertices) { return std::unexpected(ClusterError::InvalidIndex); }
  }
  return bounds;
}

uint32_t Spread(uint32_t bits) {
  bits &= kMortonBits;
  bits = (bits | (bits << 16u)) & kMortonSpread16;
  bits = (bits | (bits << 8u)) & kMortonSpread8;
  bits = (bits | (bits << 4u)) & kMortonSpread4;
  return (bits | (bits << 2u)) & kMortonSpread2;
}

uint32_t Morton(const Vec3 &point, const Bounds &bounds) {
  std::array<uint32_t, 3> quantized{};
  for (size_t axis = 0; axis < 3; ++axis) {
    const double width = bounds.High[axis] - bounds.Low[axis];
    const double fraction = width > 0 ? (point[axis] - bounds.Low[axis]) / width : 0;
    quantized[axis] = static_cast<uint32_t>(std::clamp(fraction, 0.0, 1.0) * kMortonSteps);
  }
  return (Spread(quantized[0]) << 2u) | (Spread(quantized[1]) << 1u) | Spread(quantized[2]);
}

struct OrderedTriangle {
  uint32_t Code;
  uint32_t Triangle;
};

std::vector<OrderedTriangle> Order(ClusterMeshInput input, const Bounds &bounds) {
  std::vector<OrderedTriangle> ordered;
  ordered.reserve(input.Indices.size() / kTriangleCorners);
  for (size_t first = 0; first < input.Indices.size(); first += kTriangleCorners) {
    Vec3 center;
    for (size_t corner = 0; corner < kTriangleCorners; ++corner) {
      center = center + Position(input, input.Indices[first + corner]) * (1.0 / 3.0);
    }
    ordered.push_back({.Code = Morton(center, bounds),
                       .Triangle = static_cast<uint32_t>(first / kTriangleCorners)});
  }
  std::ranges::sort(ordered, [](const auto &left, const auto &right) {
    return left.Code != right.Code ? left.Code < right.Code : left.Triangle < right.Triangle;
  });
  return ordered;
}

std::expected<DagCluster, ClusterError> BoundCluster(ClusterMeshInput input,
                                                     std::span<const uint32_t> indices) {
  Bounds bounds;
  for (const uint32_t index : indices) { bounds.Include(Position(input, index)); }
  DagCluster cluster{};
  double radiusSquared = 0;
  for (size_t axis = 0; axis < 3; ++axis) {
    cluster.SelfCenter[axis] = static_cast<float>((bounds.Low[axis] + bounds.High[axis]) * 0.5);
    const double extent = std::max(std::abs(bounds.Low[axis] - cluster.SelfCenter[axis]),
                                   std::abs(bounds.High[axis] - cluster.SelfCenter[axis]));
    radiusSquared += extent * extent;
  }
  const double radius = std::sqrt(radiusSquared);
  if (radius > std::numeric_limits<float>::max()) {
    return std::unexpected(ClusterError::BoundsOverflow);
  }
  cluster.SelfRadius = static_cast<float>(radius);
  if (cluster.SelfRadius > 0) {
    cluster.SelfRadius = std::nextafter(cluster.SelfRadius, std::numeric_limits<float>::infinity());
  }
  if (!std::isfinite(cluster.SelfRadius)) { return std::unexpected(ClusterError::BoundsOverflow); }
  cluster.ParentErr = kDagRootErr;
  cluster.Count = static_cast<uint32_t>(indices.size());
  return cluster;
}
}

std::expected<ClusteredMesh, ClusterError> CookClusters(ClusterMeshInput input,
                                                        uint32_t triangleLimit) {
  const auto bounds = Validate(input, triangleLimit);
  if (!bounds) { return std::unexpected(bounds.error()); }
  ClusteredMesh result;
  if (input.Indices.empty()) { return result; }
  if (input.Indices.size() / kTriangleCorners <= triangleLimit) {
    const auto cluster = BoundCluster(input, input.Indices);
    if (!cluster) { return std::unexpected(cluster.error()); }
    result.Index.assign(input.Indices.begin(), input.Indices.end());
    result.Clusters.push_back(*cluster);
    return result;
  }
  const auto ordered = Order(input, *bounds);
  result.Index.reserve(input.Indices.size());
  result.Clusters.reserve(ordered.size() / triangleLimit +
                          (ordered.size() % triangleLimit != 0 ? 1u : 0u));
  for (size_t first = 0; first < ordered.size();) {
    const size_t count = std::min(static_cast<size_t>(triangleLimit), ordered.size() - first);
    const auto firstIndex = static_cast<uint32_t>(result.Index.size());
    for (size_t at = first; at < first + count; ++at) {
      const size_t index = static_cast<size_t>(ordered[at].Triangle) * kTriangleCorners;
      result.Index.insert(result.Index.end(),
                          input.Indices.begin() + static_cast<ptrdiff_t>(index),
                          input.Indices.begin() + static_cast<ptrdiff_t>(index + kTriangleCorners));
    }
    auto cluster = BoundCluster(input, std::span<const uint32_t>(result.Index).subspan(firstIndex));
    if (!cluster) { return std::unexpected(cluster.error()); }
    cluster->First = firstIndex;
    result.Clusters.push_back(*cluster);
    first += count;
  }
  return result;
}
}
