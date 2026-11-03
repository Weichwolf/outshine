#include "ClipmapRing.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <vector>

namespace outshine::Render {
namespace {
constexpr float kMorphCells = 8.0f;

int32_t Wrap(int64_t node, uint32_t side) {
  const int64_t part = node % side;
  return static_cast<int32_t>(part < 0 ? part + side : part);
}

bool FiniteFloat(double value) {
  return std::isfinite(value) && std::abs(value) <= std::numeric_limits<float>::max();
}

struct GridBinding {
  ClipmapNode Origin;
  ClipmapNode NormalOrigin;
  std::optional<ClipmapNode> Parent;
};

std::expected<GridBinding, ClipmapRingError> BindGrids(const ClipmapHeightCache &heights,
                                                       const ClipmapHeightCache &normalHeights,
                                                       uint32_t level) {
  const auto origin = heights.Origin(level);
  const auto normalOrigin = normalHeights.Origin(level);
  if (!origin || !normalOrigin || !heights.Complete(level) || !normalHeights.Complete(level)) {
    return std::unexpected(ClipmapRingError::Unpublished);
  }
  const auto parent = heights.Origin(level + 1);
  if (level + 1 < heights.Grid().Levels && !parent) {
    return std::unexpected(ClipmapRingError::Unpublished);
  }
  return GridBinding{.Origin = *origin, .NormalOrigin = *normalOrigin, .Parent = parent};
}

std::expected<ClipmapRing, ClipmapRingError> Ring(const ClipmapHeightCache &heights,
                                                  const ClipmapHeightCache &normalHeights,
                                                  const ClipmapView &view,
                                                  uint32_t level) {
  const auto grids = BindGrids(heights, normalHeights, level);
  if (!grids) { return std::unexpected(grids.error()); }
  const double step = std::ldexp(heights.Grid().CellM, static_cast<int>(level));
  const auto half = static_cast<float>(heights.Grid().HalfCells);
  ClipmapRing ring;
  if (!FiniteFloat(step) || step < std::numeric_limits<float>::min()) {
    return std::unexpected(ClipmapRingError::InvalidView);
  }
  ring.CentreStep[2] = static_cast<float>(step);
  ring.CentreStep[3] = level + 1 < heights.Grid().Levels ? 1.0f : 0.0f;
  ring.Morph[2] = half - 2;
  ring.Morph[3] = std::min(kMorphCells, half / 2 - 3);
  ring.PreviousMorph[2] = ring.Morph[2];
  ring.PreviousMorph[3] = ring.Morph[3];
  ring.HeightAddress = {0, 0, static_cast<int32_t>(heights.Side()), static_cast<int32_t>(level)};
  ring.NormalAddress = {
      0, 0, static_cast<int32_t>(normalHeights.Side()), static_cast<int32_t>(level)};
  const auto parent = heights.Origin(level + 1);
  if (ring.CentreStep[3] > 0 && !parent) { return std::unexpected(ClipmapRingError::Unpublished); }
  for (size_t axis = 0; axis < 2; ++axis) {
    const int64_t centre = grids->Origin[axis] + heights.Grid().HalfCells + 1;
    const int64_t normalCentre = grids->NormalOrigin[axis] + normalHeights.Grid().HalfCells + 1;
    const double world = static_cast<double>(centre) * step;
    const double relative = (view.Camera[axis] - world) / step;
    const double previous = (view.PreviousCamera[axis] - world) / step;
    const size_t shiftAxis = axis == 0 ? 0 : 2;
    const double shiftSign = axis == 0 ? 1 : -1;
    const double currentCentre = world + shiftSign * view.Shift[shiftAxis];
    const double previousCentre = world + shiftSign * view.PreviousShift[shiftAxis];
    if (!FiniteFloat(world) || !FiniteFloat(currentCentre) || !FiniteFloat(previousCentre) ||
        !FiniteFloat(previous) || !std::isfinite(relative) || relative < 0 || relative >= 2) {
      return std::unexpected(ClipmapRingError::InvalidView);
    }
    ring.CentreStep[axis] = static_cast<float>(world);
    ring.RelativeCentres[axis] = static_cast<float>(currentCentre);
    ring.RelativeCentres[axis + 2] = static_cast<float>(previousCentre);
    ring.Morph[axis] = static_cast<float>(relative);
    ring.PreviousMorph[axis] = static_cast<float>(previous);
    ring.HeightAddress[axis] = Wrap(centre, heights.Side());
    ring.NormalAddress[axis] = Wrap(normalCentre, normalHeights.Side());
    const int64_t normalHalf = normalCentre / 2;
    if (normalHalf != centre && normalHalf != centre + 1) {
      return std::unexpected(ClipmapRingError::IncompatibleGrids);
    }
    ring.Offsets[axis + 2] = normalHalf == centre ? 0 : -2;
    if (grids->Parent) {
      const int64_t parentCentre = (*grids->Parent)[axis] + heights.Grid().HalfCells + 1;
      const int64_t phase = centre / 2 - parentCentre;
      if (phase < 0 || phase > 1) { return std::unexpected(ClipmapRingError::IncompatibleGrids); }
      ring.Offsets[axis] = static_cast<int32_t>(phase);
    }
  }
  return ring;
}
}

std::expected<std::vector<ClipmapRing>, ClipmapRingError>
MakeClipmapRings(const ClipmapHeightCache &heights,
                 const ClipmapHeightCache &normalHeights,
                 const ClipmapView &view) {
  const auto &grid = heights.Grid();
  const auto &normals = normalHeights.Grid();
  if (grid.Levels != normals.Levels || uint64_t{grid.HalfCells} * 2 != normals.HalfCells ||
      grid.CellM * 0.5 != normals.CellM ||
      normalHeights.Side() > std::numeric_limits<int32_t>::max()) {
    return std::unexpected(ClipmapRingError::IncompatibleGrids);
  }
  if (!std::ranges::all_of(view.Shift, FiniteFloat) ||
      !std::ranges::all_of(view.PreviousShift, FiniteFloat)) {
    return std::unexpected(ClipmapRingError::InvalidView);
  }
  std::vector<ClipmapRing> rings;
  rings.reserve(grid.Levels);
  for (uint32_t i = 0; i < grid.Levels; ++i) {
    auto ring = Ring(heights, normalHeights, view, i);
    if (!ring) { return std::unexpected(ring.error()); }
    rings.push_back(*ring);
  }
  return rings;
}
}
