#include "ClusterCook.h"
#include "math/Vec3.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <cstddef>
#include <vector>
#include <unordered_map>

namespace outshine {

constexpr uint32_t kMortonBits = 0x3ffu;
constexpr uint32_t kMortonSpread16 = 0x030000ffu;
constexpr uint32_t kMortonSpread8 = 0x0300f00fu;
constexpr uint32_t kMortonSpread4 = 0x030c30c3u;
constexpr uint32_t kMortonSpread2 = 0x09249249u;
constexpr float kMortonSteps = 1023.0f;

constexpr size_t kCellWordX = 73856093;
constexpr size_t kCellWordY = 19349663;
constexpr size_t kCellWordZ = 83492791;

namespace {

[[nodiscard]] uint32_t Spread(uint32_t bits) {
  bits &= kMortonBits;
  bits = (bits | (bits << 16u)) & kMortonSpread16;
  bits = (bits | (bits << 8u)) & kMortonSpread8;
  bits = (bits | (bits << 4u)) & kMortonSpread4;
  bits = (bits | (bits << 2u)) & kMortonSpread2;
  return bits;
}

[[nodiscard]] uint32_t Morton(const Vec3f &at, const Vec3f &least, const Vec3f &span) {
  std::array<uint32_t, 3> held{};
  for (int axis = 0; axis < 3; ++axis) {
    const float part = span[axis] > 0.0f ? (at[axis] - least[axis]) / span[axis] : 0.0f;
    const float held01 = std::clamp(part, 0.0f, 1.0f);
    held[axis] = static_cast<uint32_t>(held01 * kMortonSteps);
  }
  return (Spread(held[0]) << 2u) | (Spread(held[1]) << 1u) | Spread(held[2]);
}

} // namespace

Cooked CookClusters(std::span<const float> positionsM,
                    std::span<const uint32_t> indices,
                    uint32_t mostTriangles,
                    int strideFloats) {
  Cooked out;
  const size_t triangles = indices.size() / 3;
  const auto stride = static_cast<size_t>(strideFloats < 3 ? 3 : strideFloats);
  if (triangles == 0 || positionsM.size() < 3 || mostTriangles == 0) { return out; }
  const auto axisOf = [&](size_t vertex, int axis) {
    return positionsM[vertex * stride + static_cast<size_t>(axis)];
  };

  Vec3f least = {{positionsM[0], positionsM[1], positionsM[2]}};
  Vec3f most = {{positionsM[0], positionsM[1], positionsM[2]}};
  for (size_t vertex = 0; vertex * stride + 2 < positionsM.size(); ++vertex) {
    for (int axis = 0; axis < 3; ++axis) {
      const float held = axisOf(vertex, axis);
      least[axis] = held < least[axis] ? held : least[axis];
      most[axis] = held > most[axis] ? held : most[axis];
    }
  }
  const Vec3f span = most - least;

  struct Sorted {
    uint32_t Code = 0;
    uint32_t Triangle = 0;
  };

  std::vector<Sorted> order;
  order.reserve(triangles);
  const size_t vertices = positionsM.size() / stride;
  for (size_t triangle = 0; triangle < triangles; ++triangle) {
    Vec3f centre;
    for (int corner = 0; corner < 3; ++corner) {
      const uint32_t corner3 = indices[triangle * 3 + static_cast<size_t>(corner)];
      if (static_cast<size_t>(corner3) >= vertices) { continue; }
      for (int axis = 0; axis < 3; ++axis) {
        centre[axis] += axisOf(static_cast<size_t>(corner3), axis) / 3.0f;
      }
    }
    order.push_back(
        Sorted{.Code = Morton(centre, least, span), .Triangle = static_cast<uint32_t>(triangle)});
  }
  std::ranges::sort(order, [](const Sorted &a, const Sorted &b) { return a.Code < b.Code; });

  out.Index.reserve(indices.size());
  out.Clusters.reserve((triangles + mostTriangles - 1) / mostTriangles);
  for (size_t at = 0; at < order.size(); at += mostTriangles) {
    const size_t upTo = at + mostTriangles < order.size() ? at + mostTriangles : order.size();
    DagCluster made{};
    made.First = static_cast<uint32_t>(out.Index.size());
    made.Count = static_cast<uint32_t>((upTo - at) * 3);
    made.ParentErr = kDagRootErr;

    Vec3f low;
    Vec3f high;
    bool any = false;
    for (size_t step = at; step < upTo; ++step) {
      const uint32_t triangle = order[step].Triangle;
      for (int corner = 0; corner < 3; ++corner) {
        const uint32_t index =
            indices[static_cast<size_t>(triangle) * 3 + static_cast<size_t>(corner)];
        out.Index.push_back(index);
        if (static_cast<size_t>(index) >= vertices) { continue; }
        for (int axis = 0; axis < 3; ++axis) {
          const float held = axisOf(static_cast<size_t>(index), axis);
          if (!any || held < low[axis]) { low[axis] = held; }
          if (!any || held > high[axis]) { high[axis] = held; }
        }
        any = true;
      }
    }
    double radius = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
      made.SelfCenter[axis] = 0.5f * (low[axis] + high[axis]);
      const double half = 0.5 * (static_cast<double>(high[axis]) - static_cast<double>(low[axis]));
      radius += half * half;
    }
    made.SelfRadius = static_cast<float>(std::sqrt(radius));
    out.Clusters.push_back(made);
  }
  return out;
}

} // namespace outshine

namespace outshine {
namespace {

struct Cell {
  std::array<int, 3> At = {{0, 0, 0}};

  [[nodiscard]] bool operator==(const Cell &other) const {
    return At[0] == other.At[0] && At[1] == other.At[1] && At[2] == other.At[2];
  }
};

struct CellHash {
  [[nodiscard]] size_t operator()(const Cell &of) const {
    return static_cast<size_t>(of.At[0] * kCellWordX) ^ static_cast<size_t>(of.At[1] * kCellWordY) ^
           static_cast<size_t>(of.At[2] * kCellWordZ);
  }
};

} // namespace

Cooked CookDag(std::span<const float> positionsM,
               std::span<const uint32_t> indices,
               Limits within,
               int strideFloats) {
  const uint32_t mostTriangles = within.MostTriangles;
  const uint32_t mostLevels = within.MostLevels;
  const auto stride = static_cast<size_t>(strideFloats < 3 ? 3 : strideFloats);
  Cooked out = CookClusters(positionsM, indices, mostTriangles, strideFloats);

  out.PositionsM.resize(positionsM.size() / stride * 3);
  for (size_t vertex = 0; vertex * stride + 2 < positionsM.size(); ++vertex) {
    for (size_t axis = 0; axis < 3; ++axis) {
      out.PositionsM[vertex * 3 + axis] = positionsM[vertex * stride + axis];
    }
  }
  out.FirstOwnVertex = static_cast<uint32_t>(positionsM.size() / stride);
  if (out.Clusters.empty() || mostLevels == 0) { return out; }

  std::vector<uint32_t> coarseIndex(out.Index.begin(), out.Index.end());
  size_t firstOfLevel = 0;
  for (uint32_t level = 1; level <= mostLevels; ++level) {
    if (coarseIndex.size() < 3) { break; }

    Vec3f least = {{out.PositionsM[0], out.PositionsM[1], out.PositionsM[2]}};
    Vec3f most = {{least[0], least[1], least[2]}};
    for (const uint32_t index : coarseIndex) {
      for (int axis = 0; axis < 3; ++axis) {
        const float held =
            out.PositionsM[static_cast<size_t>(index) * 3 + static_cast<size_t>(axis)];
        least[axis] = held < least[axis] ? held : least[axis];
        most[axis] = held > most[axis] ? held : most[axis];
      }
    }
    float widest = 0.0f;
    for (int axis = 0; axis < 3; ++axis) {
      const float span = most[axis] - least[axis];
      widest = span > widest ? span : widest;
    }
    if (!(widest > 0.0f)) { break; }
    std::vector<uint32_t> distinct(coarseIndex.begin(), coarseIndex.end());
    std::ranges::sort(distinct);
    distinct.erase(std::ranges::unique(distinct).begin(), distinct.end());
    const double spacingM =
        static_cast<double>(widest) /
        std::sqrt(static_cast<double>(distinct.size() > 1 ? distinct.size() : 2));
    const float cellM = static_cast<float>(spacingM * 2.0) * static_cast<float>(1u << (level - 1));

    std::unordered_map<Cell, uint32_t, CellHash> stood;
    std::vector<uint32_t> standsFor(coarseIndex.size(), 0u);
    const auto firstMade = static_cast<uint32_t>(out.PositionsM.size() / 3);
    std::vector<double> summed;
    std::vector<uint32_t> counted;
    for (size_t at = 0; at < coarseIndex.size(); ++at) {
      const uint32_t index = coarseIndex[at];
      Cell where;
      for (int axis = 0; axis < 3; ++axis) {
        where.At[axis] = static_cast<int>(
            std::floor((out.PositionsM[static_cast<size_t>(index) * 3 + static_cast<size_t>(axis)] -
                        least[axis]) /
                       cellM));
      }
      auto found = stood.find(where);
      if (found == stood.end()) {
        found = stood.emplace(where, static_cast<uint32_t>(counted.size())).first;
        counted.push_back(0u);
        summed.insert(summed.end(), {0.0, 0.0, 0.0});
      }
      const uint32_t slot = found->second;
      standsFor[at] = slot;
      for (int axis = 0; axis < 3; ++axis) {
        summed[static_cast<size_t>(slot) * 3 + static_cast<size_t>(axis)] +=
            out.PositionsM[static_cast<size_t>(index) * 3 + static_cast<size_t>(axis)];
      }
      counted[slot] += 1u;
    }
    for (size_t slot = 0; slot < counted.size(); ++slot) {
      for (int axis = 0; axis < 3; ++axis) {
        out.PositionsM.push_back(
            static_cast<float>(summed[slot * 3 + static_cast<size_t>(axis)] /
                               static_cast<double>(counted[slot] > 0 ? counted[slot] : 1)));
      }
    }

    std::vector<double> nearest(counted.size(), std::numeric_limits<double>::max());
    std::vector<uint32_t> stoodFor(counted.size(), 0u);
    for (size_t at = 0; at < coarseIndex.size(); ++at) {
      const uint32_t index = coarseIndex[at];
      const uint32_t slot = standsFor[at];
      double away = 0.0;
      for (size_t axis = 0; axis < 3; ++axis) {
        const double gap =
            static_cast<double>(out.PositionsM[static_cast<size_t>(index) * 3 + axis]) -
            static_cast<double>(out.PositionsM[(static_cast<size_t>(firstMade) + slot) * 3 + axis]);
        away += gap * gap;
      }
      if (away < nearest[slot]) {
        nearest[slot] = away;
        stoodFor[slot] = index;
      }
    }
    out.MadeFrom.insert(out.MadeFrom.end(), stoodFor.begin(), stoodFor.end());

    double worst = 0.0;
    for (size_t at = 0; at < coarseIndex.size(); ++at) {
      const uint32_t index = coarseIndex[at];
      const uint32_t made = firstMade + standsFor[at];
      double away = 0.0;
      for (int axis = 0; axis < 3; ++axis) {
        const double held =
            static_cast<double>(
                out.PositionsM[static_cast<size_t>(index) * 3 + static_cast<size_t>(axis)]) -
            static_cast<double>(
                out.PositionsM[static_cast<size_t>(made) * 3 + static_cast<size_t>(axis)]);
        away += held * held;
      }
      const double moved = std::sqrt(away);
      worst = moved > worst ? moved : worst;
    }

    std::vector<uint32_t> kept;
    kept.reserve(coarseIndex.size());
    for (size_t triangle = 0; triangle + 2 < coarseIndex.size(); triangle += 3) {
      const uint32_t a = firstMade + standsFor[triangle];
      const uint32_t b = firstMade + standsFor[triangle + 1];
      const uint32_t c = firstMade + standsFor[triangle + 2];
      if (a == b || b == c || a == c) { continue; }
      kept.insert(kept.end(), {a, b, c});
    }
    if (kept.empty()) { break; }

    const Cooked above = CookClusters(out.PositionsM, kept, mostTriangles);
    if (above.Clusters.empty()) { break; }

    Vec3f wholeCentre;
    float wholeRadius = 0.0f;
    {
      Vec3f low;
      Vec3f high;
      for (int axis = 0; axis < 3; ++axis) {
        low[axis] = out.PositionsM[static_cast<size_t>(kept[0]) * 3 + static_cast<size_t>(axis)];
        high[axis] = low[axis];
      }
      for (const uint32_t index : kept) {
        for (int axis = 0; axis < 3; ++axis) {
          const float held =
              out.PositionsM[static_cast<size_t>(index) * 3 + static_cast<size_t>(axis)];
          low[axis] = held < low[axis] ? held : low[axis];
          high[axis] = held > high[axis] ? held : high[axis];
        }
      }
      double radius = 0.0;
      for (int axis = 0; axis < 3; ++axis) {
        wholeCentre[axis] = 0.5f * (low[axis] + high[axis]);
        const double half =
            0.5 * (static_cast<double>(high[axis]) - static_cast<double>(low[axis]));
        radius += half * half;
      }
      wholeRadius = static_cast<float>(std::sqrt(radius));
    }
    for (size_t at = firstOfLevel; at < out.Clusters.size(); ++at) {
      DagCluster &child = out.Clusters[at];
      for (int axis = 0; axis < 3; ++axis) { child.ParentCenter[axis] = wholeCentre[axis]; }
      child.ParentRadius = wholeRadius;
      child.ParentErr = static_cast<float>(worst);
    }

    firstOfLevel = out.Clusters.size();
    const auto rebase = static_cast<uint32_t>(out.Index.size());
    for (const DagCluster &one : above.Clusters) {
      DagCluster carried = one;
      carried.First = rebase + one.First;
      carried.Level = level;
      carried.SelfErr = static_cast<float>(worst);
      out.Clusters.push_back(carried);
    }
    out.Index.insert(out.Index.end(), above.Index.begin(), above.Index.end());
    coarseIndex = above.Index;
  }
  return out;
}

} // namespace outshine
