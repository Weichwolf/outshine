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

struct Surface {
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

[[nodiscard]] inline Census CensusOver(std::span<const Surface> made) {
  std::vector<size_t> firstVertex(made.size() + 1, 0);
  std::vector<size_t> firstTriangle(made.size() + 1, 0);
  for (size_t at = 0; at < made.size(); ++at) {
    firstVertex[at + 1] = firstVertex[at] + made[at].PlacesM.size() / 3;
    firstTriangle[at + 1] = firstTriangle[at] + made[at].Run.size() / 3;
  }
  Census out;
  out.Vertices = firstVertex.back();
  const size_t triangles = firstTriangle.back();

  const auto partOfVertex = [&](size_t one) {
    size_t at = 0;
    while (at + 1 < made.size() && one >= firstVertex[at + 1]) { ++at; }
    return at;
  };
  const auto placeAt = [&](size_t one) {
    const size_t part = partOfVertex(one);
    return made[part].PlacesM.data() + (one - firstVertex[part]) * 3;
  };
  const auto turnAt = [&](size_t one) {
    const size_t part = partOfVertex(one);
    return made[part].Facing.data() + (one - firstVertex[part]) * 3;
  };
  const auto cornerOf = [&](size_t tri, size_t corner) -> size_t {
    size_t at = 0;
    while (at + 1 < made.size() && tri >= firstTriangle[at + 1]) { ++at; }
    return firstVertex[at] + made[at].Run[(tri - firstTriangle[at]) * 3 + corner];
  };

  struct AtCm {
    int64_t X = 0, Y = 0, Z = 0;

    bool operator==(const AtCm &other) const noexcept {
      return X == other.X && Y == other.Y && Z == other.Z;
    }
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

  std::unordered_map<AtCm, uint32_t, AtCmHash> seenAt;
  std::vector<uint32_t> welded;
  welded.reserve(out.Vertices);
  for (size_t one = 0; one < out.Vertices; ++one) {
    const float *const held = placeAt(one);
    const auto cx = static_cast<int64_t>(std::llround(static_cast<double>(held[0]) * 100.0));
    const auto cy = static_cast<int64_t>(std::llround(static_cast<double>(held[1]) * 100.0));
    const auto cz = static_cast<int64_t>(std::llround(static_cast<double>(held[2]) * 100.0));
    const AtCm key{.X = cx, .Y = cy, .Z = cz};
    const auto found = seenAt.find(key);
    if (found == seenAt.end()) {
      const auto stood = static_cast<uint32_t>(seenAt.size());
      seenAt.emplace(key, stood);
      welded.push_back(stood);
    } else {
      ++out.Coincident;
      welded.push_back(found->second);
    }
  }
  out.Apart = seenAt.size();

  std::unordered_map<uint64_t, int> edges;
  for (size_t tri = 0; tri < triangles; ++tri) {
    const std::array<uint32_t, 3> corner = {
        {welded[cornerOf(tri, 0)], welded[cornerOf(tri, 1)], welded[cornerOf(tri, 2)]}};
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
    if (one.second == 1) {
      ++out.Open;
    } else if (one.second > 2) {
      ++out.Overused;
    }
  }
  out.Edges = edges.size();

  struct Corner {
    std::array<uint32_t, 6> Bits = {{0, 0, 0, 0, 0, 0}};

    bool operator==(const Corner &other) const noexcept {
      for (size_t part = 0; part < 6; ++part) {
        if (Bits[part] != other.Bits[part]) { return false; }
      }
      return true;
    }
  };

  struct CornerHash {
    size_t operator()(const Corner &of) const noexcept {
      size_t mixed = kDigestBasis;
      for (const uint32_t one : of.Bits) { mixed = (mixed ^ one) * kDigestPrime; }
      return mixed;
    }
  };

  std::unordered_map<Corner, uint32_t, CornerHash> whole;
  for (size_t one = 0; one < out.Vertices; ++one) {
    const float *const held = placeAt(one);
    const float *const aim = turnAt(one);
    Corner key;
    for (size_t part = 0; part < 3; ++part) {
      key.Bits[part] = std::bit_cast<uint32_t>(held[part]);
      key.Bits[part + 3] = std::bit_cast<uint32_t>(aim[part]);
    }
    if (whole.emplace(key, static_cast<uint32_t>(whole.size())).second) { continue; }
    ++out.Identical;
  }
  out.Distinct = whole.size();
  return out;
}

} // namespace outshine
#endif
