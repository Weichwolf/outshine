#include <span>
#include "TriangleBvh.h"

#include <optional>

#include "math/Box.h"
#include "math/Vec3.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>
#include <utility>

namespace outshine {
namespace {

constexpr float kParallelRay = 1.0e-20f;

constexpr int kBins = 12;

constexpr uint32_t kMaxLeafBits = 8;
constexpr uint32_t kMaxLeafTriangles = (1u << kMaxLeafBits) - 1u;

using Box = Boxf;

struct RayInterval {
  float NearM;
  float FarM;
};

struct PrimitiveBounds {
  Box Triangles;
  Box Centroids;
};

[[nodiscard]] Vec3f VertexAt(std::span<const float> positions, uint32_t index) noexcept {
  const size_t first = static_cast<size_t>(index) * 3;
  return {{positions[first], positions[first + 1], positions[first + 2]}};
}

[[nodiscard]] BvhTriangle TriangleAt(std::span<const float> positions,
                                     std::span<const uint32_t> indices,
                                     size_t first) noexcept {
  const Vec3f origin = VertexAt(positions, indices[first]);
  return {.V0 = origin,
          .E1 = VertexAt(positions, indices[first + 1]) - origin,
          .E2 = VertexAt(positions, indices[first + 2]) - origin};
}

[[nodiscard]] bool ValidGeometry(std::span<const float> positions,
                                 std::span<const uint32_t> indices) noexcept {
  if (positions.size() % 3 != 0 || indices.size() % 3 != 0) { return false; }
  if (!std::ranges::all_of(positions, [](float value) { return std::isfinite(value); })) {
    return false;
  }
  const size_t vertices = positions.size() / 3;
  if (!std::ranges::all_of(indices, [vertices](uint32_t index) { return index < vertices; })) {
    return false;
  }
  for (size_t first = 0; first < indices.size(); first += 3) {
    const BvhTriangle triangle = TriangleAt(positions, indices, first);
    for (size_t axis = 0; axis < 3; ++axis) {
      if (!std::isfinite(triangle.E1[axis]) || !std::isfinite(triangle.E2[axis]) ||
          !std::isfinite(triangle.V0[axis] + triangle.E1[axis]) ||
          !std::isfinite(triangle.V0[axis] + triangle.E2[axis])) {
        return false;
      }
    }
  }
  return true;
}

void UpdateNodeBounds(std::span<BvhNode> nodes,
                      std::span<const BvhTriangle> triangles,
                      size_t index) {
  BvhNode &node = nodes[index];
  Box bounds;
  if (node.IsLeaf()) {
    for (uint32_t which = 0; which < node.TriangleCount(); ++which) {
      const BvhTriangle &triangle = triangles[node.FirstTriangle() + which];
      bounds.Cover(triangle.V0);
      bounds.Cover(triangle.V0 + triangle.E1);
      bounds.Cover(triangle.V0 + triangle.E2);
    }
  } else {
    const size_t left = index + 1;
    const uint32_t right = nodes[left].Escape;
    bounds.Cover(nodes[left].MinM);
    bounds.Cover(nodes[left].MaxM);
    if (right != kBvhNoEscape && right < nodes.size()) {
      bounds.Cover(nodes[right].MinM);
      bounds.Cover(nodes[right].MaxM);
    }
  }
  node.MinM = bounds.Min;
  node.MaxM = bounds.Max;
}

[[nodiscard]] bool
IntersectsTriangle(const BvhTriangle &tri, const Ray &ray, RayInterval interval) noexcept {
  const Vec3f &originM = ray.OriginM;
  const Vec3f &direction = ray.Toward;
  const Vec3f pvec = {{direction[1] * tri.E2[2] - direction[2] * tri.E2[1],
                       direction[2] * tri.E2[0] - direction[0] * tri.E2[2],
                       direction[0] * tri.E2[1] - direction[1] * tri.E2[0]}};
  const float determinant = tri.E1[0] * pvec[0] + tri.E1[1] * pvec[1] + tri.E1[2] * pvec[2];
  if (std::fabs(determinant) < kParallelRay) { return false; }
  const float reciprocal = 1.0f / determinant;
  const Vec3f tvec = {{originM[0] - tri.V0[0], originM[1] - tri.V0[1], originM[2] - tri.V0[2]}};
  const float u = (tvec[0] * pvec[0] + tvec[1] * pvec[1] + tvec[2] * pvec[2]) * reciprocal;
  if (u < 0.0f || u > 1.0f) { return false; }
  const Vec3f qvec = {{tvec[1] * tri.E1[2] - tvec[2] * tri.E1[1],
                       tvec[2] * tri.E1[0] - tvec[0] * tri.E1[2],
                       tvec[0] * tri.E1[1] - tvec[1] * tri.E1[0]}};
  const float v =
      (direction[0] * qvec[0] + direction[1] * qvec[1] + direction[2] * qvec[2]) * reciprocal;
  if (v < 0.0f || u + v > 1.0f) { return false; }
  const float hit = (tri.E2[0] * qvec[0] + tri.E2[1] * qvec[1] + tri.E2[2] * qvec[2]) * reciprocal;
  return hit > interval.NearM && hit < interval.FarM;
}

[[nodiscard]] bool
IntersectsBounds(const BvhNode &node, const Ray &ray, RayInterval interval) noexcept {
  double enter = interval.NearM;
  double leave = interval.FarM;
  for (size_t axis = 0; axis < 3; ++axis) {
    if (ray.Toward[axis] == 0.0f) {
      if (ray.OriginM[axis] < node.MinM[axis] || ray.OriginM[axis] > node.MaxM[axis]) {
        return false;
      }
      continue;
    }
    const double inverse = 1.0 / ray.Toward[axis];
    const double first = (static_cast<double>(node.MinM[axis]) - ray.OriginM[axis]) * inverse;
    const double second = (static_cast<double>(node.MaxM[axis]) - ray.OriginM[axis]) * inverse;
    enter = std::max(enter, std::min(first, second));
    leave = std::min(leave, std::max(first, second));
    if (enter > leave) { return false; }
  }
  return true;
}

struct Building {
  std::vector<Box> Bounds;
  std::vector<Vec3f> Centroids;
  std::vector<uint32_t> Order;
  std::vector<BvhNode> Nodes;

  std::vector<uint32_t> Right;
  uint32_t Depth = 0;
};

[[nodiscard]] uint32_t PartitionBySurfaceArea(Building &work,
                                              uint32_t first,
                                              uint32_t count,
                                              const PrimitiveBounds &bounds) {
  const Box &box = bounds.Triangles;
  const Box &centroidBox = bounds.Centroids;
  uint32_t split = 0;
  int axis = 0;
  float widest = centroidBox.Max[0] - centroidBox.Min[0];
  for (int candidate = 1; candidate < 3; ++candidate) {
    const float width = centroidBox.Max[candidate] - centroidBox.Min[candidate];
    if (width > widest) {
      widest = width;
      axis = candidate;
    }
  }
  if (widest > 0.0f && std::isfinite(widest)) {
    const double scale = static_cast<double>(kBins) / widest;
    std::array<Box, kBins> binBox{};
    std::array<uint32_t, kBins> binCount = {{}};
    const auto BinOf = [&](uint32_t tri) {
      const double offset = static_cast<double>(work.Centroids[tri][static_cast<size_t>(axis)]) -
                            centroidBox.Min[axis];
      const int at = static_cast<int>(offset * scale);
      return std::min(std::max(at, 0), kBins - 1);
    };
    for (uint32_t at = 0; at < count; ++at) {
      const uint32_t tri = work.Order[first + at];
      const int bin = BinOf(tri);
      binBox[bin].Cover(work.Bounds[tri]);
      ++binCount[bin];
    }

    std::array<float, kBins - 1> leftCost = {};
    std::array<float, kBins - 1> rightCost = {};
    Box sweep;
    uint32_t running = 0;
    for (int bin = 0; bin < kBins - 1; ++bin) {
      sweep.Cover(binBox[bin]);
      running += binCount[bin];
      leftCost[bin] = sweep.HalfArea() * static_cast<float>(running);
    }
    sweep = Box();
    running = 0;
    for (int bin = kBins - 1; bin > 0; --bin) {
      sweep.Cover(binBox[bin]);
      running += binCount[bin];
      rightCost[bin - 1] = sweep.HalfArea() * static_cast<float>(running);
    }
    int bestPlane = -1;
    float bestCost = std::numeric_limits<float>::infinity();
    for (int bin = 0; bin < kBins - 1; ++bin) {
      const float cost = leftCost[bin] + rightCost[bin];
      if (cost < bestCost) {
        bestCost = cost;
        bestPlane = bin;
      }
    }

    const float leafCost = box.HalfArea() * static_cast<float>(count);
    if (bestPlane >= 0 && bestCost + box.HalfArea() < leafCost) {
      const auto middle = std::partition(work.Order.begin() + first,
                                         work.Order.begin() + first + count,
                                         [&](uint32_t tri) { return BinOf(tri) <= bestPlane; });
      split = static_cast<uint32_t>(middle - (work.Order.begin() + first));
    }
  }
  return split;
}

uint32_t Emit(Building &work, uint32_t first, uint32_t count, uint32_t depth) {
  const auto here = static_cast<uint32_t>(work.Nodes.size());
  work.Nodes.emplace_back();
  work.Right.push_back(0);
  work.Depth = std::max(work.Depth, depth + 1u);

  PrimitiveBounds bounds;
  for (uint32_t at = 0; at < count; ++at) {
    const uint32_t tri = work.Order[first + at];
    bounds.Triangles.Cover(work.Bounds[tri]);
    bounds.Centroids.Cover(work.Centroids[tri]);
  }

  const auto MakeLeaf = [&] { work.Nodes[here].Leaf = (count << kBvhLeafFirstBits) | first; };

  uint32_t split =
      count > kBvhLeafTriangles ? PartitionBySurfaceArea(work, first, count, bounds) : 0;

  if (split == 0 || split == count) {
    if (count <= kMaxLeafTriangles) {
      MakeLeaf();
    } else {
      split = count / 2u;
    }
  }

  if (work.Nodes[here].Leaf == kBvhInterior) {
    Emit(work, first, split, depth + 1u);

    const uint32_t right = Emit(work, first + split, count - split, depth + 1u);
    work.Right[here] = right;
  }

  work.Nodes[here].MinM = bounds.Triangles.Min;
  work.Nodes[here].MaxM = bounds.Triangles.Max;
  return here;
}

void Thread(Building &work, uint32_t here, uint32_t escape) {
  work.Nodes[here].Escape = escape;
  if (work.Nodes[here].IsLeaf()) { return; }
  Thread(work, here + 1u, work.Right[here]);
  Thread(work, work.Right[here], escape);
}

}

TriangleBvh TriangleBvh::Over(std::span<const float> positionsM,
                              std::span<const uint32_t> indices) {
  TriangleBvh built;
  const size_t triangles = indices.size() / 3u;
  if (triangles == 0 || indices.size() % 3u != 0 || triangles > kBvhLeafFirstMask) { return built; }
  if (!ValidGeometry(positionsM, indices)) { return built; }
  Building work;
  work.Bounds.resize(triangles);
  work.Centroids.resize(triangles);
  work.Order.resize(triangles);
  built.Tris_.resize(triangles);
  for (size_t tri = 0; tri < triangles; ++tri) {
    work.Order[tri] = static_cast<uint32_t>(tri);
    for (size_t corner = 0; corner < 3; ++corner) {
      work.Bounds[tri].Cover(VertexAt(positionsM, indices[tri * 3 + corner]));
    }
    work.Centroids[tri] = work.Bounds[tri].Min * 0.5f + work.Bounds[tri].Max * 0.5f;
  }

  work.Nodes.reserve(triangles * 2u);
  work.Right.reserve(triangles * 2u);
  Emit(work, 0, static_cast<uint32_t>(triangles), 0);
  Thread(work, 0, kBvhNoEscape);

  for (size_t at = 0; at < triangles; ++at) {
    built.Tris_[at] = TriangleAt(positionsM, indices, static_cast<size_t>(work.Order[at]) * 3);
  }

  built.Corners_.resize(triangles * 3u);
  for (size_t at = 0; at < triangles; ++at) {
    const uint32_t tri = work.Order[at];
    for (int corner_at = 0; corner_at < 3; ++corner_at) {
      built.Corners_[at * 3u + static_cast<size_t>(corner_at)] =
          indices[static_cast<size_t>(tri) * 3u + static_cast<size_t>(corner_at)];
    }
  }
  built.Nodes_ = std::move(work.Nodes);
  built.Depth_ = work.Depth;
  return built;
}

bool TriangleBvh::Refit(std::span<const float> positionsM) {
  if (Nodes_.empty() || Corners_.size() != Tris_.size() * 3u) { return false; }
  if (!ValidGeometry(positionsM, Corners_)) { return false; }
  for (size_t at = 0; at < Tris_.size(); ++at) {
    Tris_[at] = TriangleAt(positionsM, Corners_, at * 3);
  }
  for (size_t at = Nodes_.size(); at > 0; --at) { UpdateNodeBounds(Nodes_, Tris_, at - 1); }

  return true;
}

std::optional<float> TriangleBvh::Under(float eastM, float southM) const {
  if (Nodes_.empty()) { return std::nullopt; }

  bool have = false;
  float highest = 0.0F;
  uint32_t at = 0;
  while (at != kBvhNoEscape) {
    const BvhNode &node = Nodes_[at];
    if (eastM < node.MinM[0] || eastM > node.MaxM[0] || southM < node.MinM[2] ||
        southM > node.MaxM[2]) {
      at = node.Escape;
      continue;
    }
    if (!node.IsLeaf()) {
      at = at + 1u;
      continue;
    }
    const uint32_t first = node.FirstTriangle();
    const uint32_t count = node.TriangleCount();
    for (uint32_t which = 0; which < count; ++which) {
      const BvhTriangle &tri = Tris_[first + which];
      const float e1x = tri.E1[0];
      const float e1z = tri.E1[2];
      const float e2x = tri.E2[0];
      const float e2z = tri.E2[2];
      const float under = e1x * e2z - e1z * e2x;
      if (under == 0.0F) { continue; }
      const float toX = eastM - tri.V0[0];
      const float toZ = southM - tri.V0[2];
      const float one = (toX * e2z - toZ * e2x) / under;
      const float two = (e1x * toZ - e1z * toX) / under;
      if (one < 0.0F || two < 0.0F || one + two > 1.0F) { continue; }
      const float heightM = tri.V0[1] + one * tri.E1[1] + two * tri.E2[1];
      if (!have || heightM > highest) {
        have = true;
        highest = heightM;
      }
    }
    at = node.Escape;
  }
  return have ? std::optional<float>(highest) : std::nullopt;
}

bool TriangleBvh::Occludes(const Ray &along, float nearM, float distanceM) const {
  if (Nodes_.empty() || !std::isfinite(nearM) || nearM < 0 || !(distanceM > nearM)) {
    return false;
  }
  bool directed = false;
  for (size_t axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(along.OriginM[axis]) || !std::isfinite(along.Toward[axis])) { return false; }
    directed = directed || along.Toward[axis] != 0.0f;
  }
  if (!directed) { return false; }
  const RayInterval interval{.NearM = nearM, .FarM = distanceM};
  uint32_t at = 0;
  while (at != kBvhNoEscape) {
    const BvhNode &node = Nodes_[at];
    if (!IntersectsBounds(node, along, interval)) {
      at = node.Escape;
      continue;
    }
    if (!node.IsLeaf()) {
      ++at;
      continue;
    }
    for (uint32_t which = 0; which < node.TriangleCount(); ++which) {
      if (IntersectsTriangle(Tris_[node.FirstTriangle() + which], along, interval)) { return true; }
    }
    at = node.Escape;
  }
  return false;
}

}
