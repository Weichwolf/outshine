#ifndef OUTSHINE_BASE_SPATIAL_HEIGHTBANDINTERNAL_H
#define OUTSHINE_BASE_SPATIAL_HEIGHTBANDINTERNAL_H

#include "HeightBandMesh.h"
#include <functional>
#include <unordered_map>

namespace outshine::HeightBands {
using Error = HeightBandError;
template <typename T> using Checked = std::expected<T, Error>;
using VertexPair = std::array<uint32_t, 2>;

struct EdgeLevel {
  VertexPair Edge;
  uint32_t Level = 0;
  bool operator==(const EdgeLevel &) const = default;
};

struct EdgeLevelHash {
  size_t operator()(const EdgeLevel &key) const {
    constexpr uint64_t mix = 0x9e3779b97f4a7c15ULL;
    const uint64_t edge = (uint64_t{key.Edge[0]} << 32) | key.Edge[1];
    return std::hash<uint64_t>{}(edge ^ (uint64_t{key.Level} * mix));
  }
};

struct Polygon {
  static constexpr size_t kMaximumCorners = 3 + 2;
  std::array<uint32_t, kMaximumCorners> Vertices{};
  size_t Count = 0;

  [[nodiscard]] Checked<void> Append(uint32_t vertex);
};

enum class HalfSpace : uint8_t { Above, Below };

struct FaceOwner {
  uint32_t Parent = 0;
  int32_t Level = -1;
};

struct State {
  const HeightBandInput &Input;
  HeightBandBudget Budget;
  const std::stop_token &Stop;
  HeightBandMesh Output;
  std::vector<VertexPair> Roots;
  std::unordered_map<EdgeLevel, uint32_t, EdgeLevelHash> Intersections;

  [[nodiscard]] Checked<void> Check() const {
    if (Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    return {};
  }
};

[[nodiscard]] Checked<void>
Validate(const HeightBandInput &input, HeightBandBudget budget, const std::stop_token &stop);
[[nodiscard]] Checked<uint32_t> Crossing(State &state, VertexPair edge, uint32_t level);
[[nodiscard]] Checked<Polygon>
Clip(State &state, const Polygon &polygon, uint32_t level, HalfSpace half);
[[nodiscard]] Checked<void> Emit(State &state, const Polygon &polygon, FaceOwner owner);
}
#endif
