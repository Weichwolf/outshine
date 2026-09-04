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

struct Building {
  std::vector<Box> Bounds;
  std::vector<Vec3f> Centroids;
  std::vector<uint32_t> Order;
  std::vector<BvhNode> Nodes;

  std::vector<uint32_t> Right;
  uint32_t Depth = 0;
};

uint32_t Emit(Building &work, uint32_t first, uint32_t count, uint32_t depth) {
  const auto here = static_cast<uint32_t>(work.Nodes.size());
  work.Nodes.emplace_back();
  work.Right.push_back(0);
  work.Depth = std::max(work.Depth, depth + 1u);

  Box box;
  Box centroidBox;
  for (uint32_t at = 0; at < count; ++at) {
    const uint32_t tri = work.Order[first + at];
    box.Cover(work.Bounds[tri]);
    centroidBox.Cover(work.Centroids[tri]);
  }

  const auto MakeLeaf = [&] { work.Nodes[here].Leaf = (count << kBvhLeafFirstBits) | first; };

  uint32_t split = 0;
  if (count > kBvhLeafTriangles) {
    int axis = 0;
    float widest = centroidBox.Max[0] - centroidBox.Min[0];
    for (int candidate = 1; candidate < 3; ++candidate) {
      const float width = centroidBox.Max[candidate] - centroidBox.Min[candidate];
      if (width > widest) {
        widest = width;
        axis = candidate;
      }
    }
    if (widest > 0.0f) {
      const float scale = static_cast<float>(kBins) / widest;
      std::array<Box, kBins> binBox{};
      std::array<uint32_t, kBins> binCount = {{}};
      const auto BinOf = [&](uint32_t tri) {
        const float offset = work.Centroids[tri][static_cast<size_t>(axis)] - centroidBox.Min[axis];
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
  }

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

  work.Nodes[here].MinM = box.Min;
  work.Nodes[here].MaxM = box.Max;
  return here;
}

void Thread(Building &work, uint32_t here, uint32_t escape) {
  work.Nodes[here].Escape = escape;
  if (work.Nodes[here].IsLeaf()) { return; }
  Thread(work, here + 1u, work.Right[here]);
  Thread(work, work.Right[here], escape);
}

} // namespace

TriangleBvh TriangleBvh::Over(std::span<const float> positionsM,
                              std::span<const uint32_t> indices) {
  TriangleBvh built;
  const size_t triangles = indices.size() / 3u;
  if (triangles == 0 || indices.size() % 3u != 0 || triangles > kBvhLeafFirstMask) { return built; }
  const size_t vertices = positionsM.size() / 3u;

  Building work;
  work.Bounds.resize(triangles);
  work.Centroids.resize(triangles);
  work.Order.resize(triangles);
  built.Tris_.resize(triangles);

  for (size_t tri = 0; tri < triangles; ++tri) {
    work.Order[tri] = static_cast<uint32_t>(tri);
    for (int corner_at = 0; corner_at < 3; ++corner_at) {
      const uint32_t vertex = indices[tri * 3u + static_cast<size_t>(corner_at)];
      Vec3f corner;
      for (int axis = 0; axis < 3; ++axis) {
        corner[static_cast<size_t>(axis)] =
            vertex < vertices
                ? positionsM[static_cast<size_t>(vertex) * 3u + static_cast<size_t>(axis)]
                : 0.0f;
      }
      work.Bounds[tri].Cover(corner);
    }
    work.Centroids[tri] = (work.Bounds[tri].Min + work.Bounds[tri].Max) * 0.5f;
  }

  work.Nodes.reserve(triangles * 2u);
  work.Right.reserve(triangles * 2u);
  Emit(work, 0, static_cast<uint32_t>(triangles), 0);
  Thread(work, 0, kBvhNoEscape);

  for (size_t at = 0; at < triangles; ++at) {
    const uint32_t tri = work.Order[at];
    std::array<std::array<float, 3>, 3> corner;
    for (int corner_at = 0; corner_at < 3; ++corner_at) {
      const uint32_t vertex =
          indices[static_cast<size_t>(tri) * 3u + static_cast<size_t>(corner_at)];
      for (int axis = 0; axis < 3; ++axis) {
        corner[corner_at][axis] =
            vertex < vertices
                ? positionsM[static_cast<size_t>(vertex) * 3u + static_cast<size_t>(axis)]
                : 0.0f;
      }
    }
    BvhTriangle &out = built.Tris_[at];
    for (int axis = 0; axis < 3; ++axis) {
      out.V0[axis] = corner[0][axis];
      out.E1[axis] = corner[1][axis] - corner[0][axis];
      out.E2[axis] = corner[2][axis] - corner[0][axis];
    }
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
  const size_t vertices = positionsM.size() / 3u;

  for (size_t at = 0; at < Tris_.size(); ++at) {
    std::array<std::array<float, 3>, 3> corner;
    for (int corner_at = 0; corner_at < 3; ++corner_at) {
      const uint32_t vertex = Corners_[at * 3u + static_cast<size_t>(corner_at)];
      for (int axis = 0; axis < 3; ++axis) {
        corner[corner_at][axis] =
            vertex < vertices
                ? positionsM[static_cast<size_t>(vertex) * 3u + static_cast<size_t>(axis)]
                : 0.0f;
      }
    }
    BvhTriangle &out = Tris_[at];
    for (int axis = 0; axis < 3; ++axis) {
      out.V0[axis] = corner[0][axis];
      out.E1[axis] = corner[1][axis] - corner[0][axis];
      out.E2[axis] = corner[2][axis] - corner[0][axis];
    }
  }

  for (size_t at = Nodes_.size(); at > 0; --at) {
    BvhNode &node = Nodes_[at - 1];
    Vec3f least;
    Vec3f most;
    bool began = false;
    const auto widen = [&least, &most, &began](const Vec3f &point) {
      for (int axis = 0; axis < 3; ++axis) {
        least[axis] = began ? std::min(point[axis], least[axis]) : point[axis];
        most[axis] = began ? std::max(point[axis], most[axis]) : point[axis];
      }
      began = true;
    };
    if (node.IsLeaf()) {
      const uint32_t first = node.FirstTriangle();
      const uint32_t count = node.TriangleCount();
      for (uint32_t which = 0; which < count; ++which) {
        const BvhTriangle &tri = Tris_[static_cast<size_t>(first) + which];
        widen(tri.V0);
        widen(tri.V0 + tri.E1);
        widen(tri.V0 + tri.E2);
      }
    } else {
      const size_t left = at;
      const uint32_t right = Nodes_[left].Escape;
      widen(Nodes_[left].MinM);
      widen(Nodes_[left].MaxM);
      if (right != kBvhNoEscape && static_cast<size_t>(right) < Nodes_.size()) {
        widen(Nodes_[right].MinM);
        widen(Nodes_[right].MaxM);
      }
    }
    if (!began) { continue; }
    for (int axis = 0; axis < 3; ++axis) {
      node.MinM[axis] = least[axis];
      node.MaxM[axis] = most[axis];
    }
  }
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
  if (Nodes_.empty()) { return false; }
  const Vec3f &originM = along.OriginM;
  const Vec3f &direction = along.Toward;
  Vec3f inverse;
  for (int axis = 0; axis < 3; ++axis) { inverse[axis] = 1.0f / direction[axis]; }

  uint32_t at = 0;
  while (at != kBvhNoEscape) {
    const BvhNode &node = Nodes_[at];
    float enter = nearM;
    float leave = distanceM;
    for (int axis = 0; axis < 3; ++axis) {
      const float first = (node.MinM[axis] - originM[axis]) * inverse[axis];
      const float second = (node.MaxM[axis] - originM[axis]) * inverse[axis];
      enter = std::max(enter, std::min(first, second));
      leave = std::min(leave, std::max(first, second));
    }
    if (enter > leave) {
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

      const Vec3f pvec = {{direction[1] * tri.E2[2] - direction[2] * tri.E2[1],
                           direction[2] * tri.E2[0] - direction[0] * tri.E2[2],
                           direction[0] * tri.E2[1] - direction[1] * tri.E2[0]}};
      const float determinant = tri.E1[0] * pvec[0] + tri.E1[1] * pvec[1] + tri.E1[2] * pvec[2];
      if (std::fabs(determinant) < kParallelRay) { continue; }
      const float reciprocal = 1.0f / determinant;
      const Vec3f tvec = {{originM[0] - tri.V0[0], originM[1] - tri.V0[1], originM[2] - tri.V0[2]}};
      const float u = (tvec[0] * pvec[0] + tvec[1] * pvec[1] + tvec[2] * pvec[2]) * reciprocal;
      if (u < 0.0f || u > 1.0f) { continue; }
      const Vec3f qvec = {{tvec[1] * tri.E1[2] - tvec[2] * tri.E1[1],
                           tvec[2] * tri.E1[0] - tvec[0] * tri.E1[2],
                           tvec[0] * tri.E1[1] - tvec[1] * tri.E1[0]}};
      const float v =
          (direction[0] * qvec[0] + direction[1] * qvec[1] + direction[2] * qvec[2]) * reciprocal;
      if (v < 0.0f || u + v > 1.0f) { continue; }
      const float hit =
          (tri.E2[0] * qvec[0] + tri.E2[1] * qvec[1] + tri.E2[2] * qvec[2]) * reciprocal;
      if (hit > nearM && hit < distanceM) { return true; }
    }
    at = node.Escape;
  }
  return false;
}

} // namespace outshine
