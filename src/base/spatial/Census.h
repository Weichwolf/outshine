#ifndef OUTSHINE_BASE_SPATIAL_CENSUS_H
#define OUTSHINE_BASE_SPATIAL_CENSUS_H

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include "Digest.h"

namespace outshine {

struct Counted {
  std::span<const float> PlacesM;
  std::span<const float> Facing;
  std::span<const uint32_t> Run;
};

struct Census {
  size_t Vertices = 0;
  size_t Apart = 0;
  size_t Coincident = 0;
  size_t Identical = 0;
  size_t Distinct = 0;
  size_t Degenerate = 0;
  size_t Open = 0;
  size_t Overused = 0;
  size_t Edges = 0;
};

struct AtCm {
  int64_t X = 0, Y = 0, Z = 0;

  bool operator==(const AtCm &other) const noexcept = default;
};

struct AtCmHash {
  size_t operator()(const AtCm &of) const noexcept {
    uint64_t mixed = kDigestBasis;
    mixed = (mixed ^ static_cast<uint64_t>(of.X)) * kDigestPrime;
    mixed = (mixed ^ static_cast<uint64_t>(of.Y)) * kDigestPrime;
    mixed = (mixed ^ static_cast<uint64_t>(of.Z)) * kDigestPrime;
    return static_cast<size_t>(mixed);
  }
};

struct Corner {
  std::array<uint32_t, 6> Bits = {{0, 0, 0, 0, 0, 0}};

  bool operator==(const Corner &other) const noexcept = default;
};

struct CornerHash {
  size_t operator()(const Corner &of) const noexcept {
    size_t mixed = kDigestBasis;
    for (const uint32_t one : of.Bits) { mixed = (mixed ^ one) * kDigestPrime; }
    return mixed;
  }
};

struct Starts {
  std::vector<size_t> FirstVertex;
  std::vector<size_t> FirstTriangle;

  explicit Starts(std::span<const Counted> made)
      : FirstVertex(made.size() + 1, 0), FirstTriangle(made.size() + 1, 0) {
    for (size_t at = 0; at < made.size(); ++at) {
      FirstVertex[at + 1] = FirstVertex[at] + made[at].PlacesM.size() / 3;
      FirstTriangle[at + 1] = FirstTriangle[at] + made[at].Run.size() / 3;
    }
  }

  [[nodiscard]] size_t PartOfVertex(size_t one, size_t surfaces) const {
    size_t at = 0;
    while (at + 1 < surfaces && one >= FirstVertex[at + 1]) { ++at; }
    return at;
  }

  [[nodiscard]] const float *PlaceAt(std::span<const Counted> made, size_t one) const {
    const size_t part = PartOfVertex(one, made.size());
    return made[part].PlacesM.data() + (one - FirstVertex[part]) * 3;
  }

  [[nodiscard]] const float *TurnAt(std::span<const Counted> made, size_t one) const {
    const size_t part = PartOfVertex(one, made.size());
    return made[part].Facing.data() + (one - FirstVertex[part]) * 3;
  }

  [[nodiscard]] size_t CornerOf(std::span<const Counted> made, size_t tri, size_t corner) const {
    size_t at = 0;
    while (at + 1 < made.size() && tri >= FirstTriangle[at + 1]) { ++at; }
    return FirstVertex[at] + made[at].Run[(tri - FirstTriangle[at]) * 3 + corner];
  }
};

inline std::vector<uint32_t>
WeldedPlaces(std::span<const Counted> made, const Starts &laid, Census &out) {
  std::unordered_map<AtCm, uint32_t, AtCmHash> seenAt;
  std::vector<uint32_t> welded;
  welded.reserve(out.Vertices);
  for (size_t one = 0; one < out.Vertices; ++one) {
    const float *const held = laid.PlaceAt(made, one);
    const AtCm key{.X = static_cast<int64_t>(std::llround(static_cast<double>(held[0]) * 100.0)),
                   .Y = static_cast<int64_t>(std::llround(static_cast<double>(held[1]) * 100.0)),
                   .Z = static_cast<int64_t>(std::llround(static_cast<double>(held[2]) * 100.0))};
    const auto found = seenAt.find(key);
    if (found == seenAt.end()) {
      const auto stood = static_cast<uint32_t>(seenAt.size());
      seenAt.emplace(key, stood);
      welded.push_back(stood);
      continue;
    }
    ++out.Coincident;
    welded.push_back(found->second);
  }
  out.Apart = seenAt.size();
  return welded;
}

inline void CountEdges(std::span<const Counted> made,
                       const Starts &laid,
                       std::span<const uint32_t> welded,
                       size_t triangles,
                       Census &out) {
  std::unordered_map<uint64_t, int> edges;
  for (size_t tri = 0; tri < triangles; ++tri) {
    const std::array<uint32_t, 3> corner = {{welded[laid.CornerOf(made, tri, 0)],
                                             welded[laid.CornerOf(made, tri, 1)],
                                             welded[laid.CornerOf(made, tri, 2)]}};
    if (corner[0] == corner[1] || corner[1] == corner[2] || corner[2] == corner[0]) {
      ++out.Degenerate;
      continue;
    }
    for (int side = 0; side < 3; ++side) {
      const uint32_t from = corner[side];
      const uint32_t to = corner[(side + 1) % 3];
      const uint64_t low = from < to ? from : to;
      const uint64_t high = from < to ? to : from;
      edges[(low << 32U) | high] += 1;
    }
  }
  for (const auto &one : edges) {
    if (one.second == 1) { ++out.Open; }
    if (one.second > 2) { ++out.Overused; }
  }
  out.Edges = edges.size();
}

inline void CountIdentical(std::span<const Counted> made, const Starts &laid, Census &out) {
  std::unordered_map<Corner, uint32_t, CornerHash> whole;
  for (size_t one = 0; one < out.Vertices; ++one) {
    const float *const held = laid.PlaceAt(made, one);
    const float *const aim = laid.TurnAt(made, one);
    Corner key;
    for (size_t part = 0; part < 3; ++part) {
      key.Bits[part] = std::bit_cast<uint32_t>(held[part]);
      key.Bits[part + 3] = std::bit_cast<uint32_t>(aim[part]);
    }
    if (whole.emplace(key, static_cast<uint32_t>(whole.size())).second) { continue; }
    ++out.Identical;
  }
  out.Distinct = whole.size();
}

[[nodiscard]] inline Census CensusOver(std::span<const Counted> made) {
  const Starts laid(made);
  Census out;
  out.Vertices = laid.FirstVertex.back();
  const std::vector<uint32_t> welded = WeldedPlaces(made, laid, out);
  CountEdges(made, laid, welded, laid.FirstTriangle.back(), out);
  CountIdentical(made, laid, out);
  return out;
}

} // namespace outshine
#endif
