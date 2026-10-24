#include "HeightBandInternal.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <utility>

namespace outshine::HeightBands {
namespace {
auto SearchLess(State &state) {
  return [&state](double a, double b) {
    ++state.Output.Work.SearchComparisons;
    return a < b;
  };
}

HeightLevelRange
Selected(State &state, std::span<const double> levels, std::array<double, 2> bounds) {
  const auto [low, high] = bounds;
  if (low <= levels[1] && high >= levels[levels.size() - 2]) {
    return {.First = 1, .Count = static_cast<uint32_t>(levels.size() - 1)};
  }
  const auto less = SearchLess(state);
  const auto first = std::ranges::lower_bound(levels.subspan(1), low, less);
  const auto end = std::ranges::upper_bound(levels.first(levels.size() - 1), high, less);
  const auto begin = static_cast<uint32_t>(first - levels.begin());
  const auto stop = static_cast<uint32_t>(end - levels.begin()) + 1;
  return {.First = begin, .Count = stop > begin ? stop - begin : 0};
}

Checked<void> Partition(State &state, uint32_t parent) {
  const auto face = state.Input.Faces[parent];
  const Polygon triangle{.Vertices = {face[0], face[1], face[2]}, .Count = face.size()};
  const auto range = state.Input.LevelRanges[parent];
  if (range.Count == 0) { return Emit(state, triangle, {.Parent = parent, .Level = -1}); }
  if (range.Count == 1) {
    return Emit(state, triangle, {.Parent = parent, .Level = static_cast<int32_t>(range.First)});
  }
  const auto levels = state.Input.Levels.subspan(range.First, range.Count);
  const std::array heights{state.Input.Points[face[0]][2],
                           state.Input.Points[face[1]][2],
                           state.Input.Points[face[2]][2]};
  const auto [low, high] = std::ranges::minmax(heights);
  if (low == high) {
    const auto level = std::ranges::lower_bound(levels, low, SearchLess(state));
    if (level == levels.end() || low < levels.front()) {
      state.Output.Work.SkippedBands += levels.size() - 1;
      return {};
    }
    ++state.Output.Work.BandVisits;
    state.Output.Work.SkippedBands += levels.size() - 2;
    return Emit(
        state,
        triangle,
        {.Parent = parent, .Level = static_cast<int32_t>(range.First + (level - levels.begin()))});
  }
  const auto selected = Selected(state, levels, {low, high});
  state.Output.Work.SkippedBands += levels.size() - 1 - selected.Count;
  for (uint32_t at = selected.First; at < selected.First + selected.Count; ++at) {
    if (auto valid = state.Check(); !valid) { return valid; }
    ++state.Output.Work.BandVisits;
    const auto first = Clip(state, triangle, range.First + at - 1, HalfSpace::Above);
    if (!first) { return std::unexpected(first.error()); }
    const auto second = Clip(state, *first, range.First + at, HalfSpace::Below);
    if (!second) { return std::unexpected(second.error()); }
    if (auto emitted = Emit(
            state, *second, {.Parent = parent, .Level = static_cast<int32_t>(range.First + at)});
        !emitted) {
      return emitted;
    }
  }
  return {};
}
}
}

namespace outshine {
std::expected<HeightBandMesh, HeightBandError>
CutHeightBands(const HeightBandInput &input, HeightBandBudget budget, const std::stop_token &stop) {
  using namespace HeightBands;
  if (auto valid = Validate(input, budget, stop); !valid) { return std::unexpected(valid.error()); }
  State state{.Input = input,
              .Budget = budget,
              .Stop = stop,
              .Output = {},
              .Roots = {},
              .Intersections = {}};
  state.Output.Points.assign(input.Points.begin(), input.Points.end());
  state.Output.Work.LevelChecks = input.Levels.size();
  state.Output.Work.CopiedInputBytes = input.Points.size_bytes();
  for (uint32_t parent = 0; parent < input.Faces.size(); ++parent) {
    if (auto valid = state.Check(); !valid) { return std::unexpected(valid.error()); }
    if (auto partitioned = Partition(state, parent); !partitioned) {
      return std::unexpected(partitioned.error());
    }
  }
  return std::move(state.Output);
}
}
