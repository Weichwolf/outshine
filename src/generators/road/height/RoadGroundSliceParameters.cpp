#include "RoadGroundMeshInternal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>

namespace outshine::Generators::RoadGround {
namespace {
constexpr double kDenominatorLeast = 1e-8;
constexpr double kParameterTolerance = 64 * std::numeric_limits<double>::epsilon();
}

double MeshArea(PlanarPoint a, PlanarPoint b, PlanarPoint c) noexcept {
  return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
}

MeshResult<Parameter>
ParameterAt(const RoadPortField::Patch &patch, PlanarPoint point, double toleranceM) noexcept {
  std::array<double, 2> signedDistance{};
  for (size_t side = 0; side < 2; ++side) {
    const auto &normal = patch.InwardNormals[side];
    const auto &centre = patch.CutCentres[side];
    signedDistance[side] =
        (point[0] - centre.EastM) * normal.EastM + (point[1] - centre.NorthM) * normal.NorthM;
  }
  Parameter value{.U = signedDistance[0] / patch.ScaleM,
                  .D = (signedDistance[0] + signedDistance[1]) / patch.ScaleM,
                  .T = 0};
  if (!std::isfinite(value.U) || !std::isfinite(value.D) || value.D <= kDenominatorLeast) {
    return std::unexpected(RoadGroundError::InvalidGeometry);
  }
  value.T = value.U / value.D;
  if (std::abs(signedDistance[0]) <= toleranceM) { value.T = 0; }
  if (std::abs(signedDistance[1]) <= toleranceM) { value.T = 1; }
  if (!std::isfinite(value.T) || value.T < 0 || value.T > 1) {
    return std::unexpected(RoadGroundError::InvalidGeometry);
  }
  return value;
}

MeshResult<void> PreparePatch(SliceState &state, size_t segments) {
  if (segments == 0 || segments + 1 > state.Budget.StationsPerPatch) {
    return std::unexpected(RoadGroundError::CapacityExceeded);
  }
  state.Knots.clear();
  state.Edges.clear();
  state.Nodes.clear();
  state.Knots.reserve(segments + 1 + 3 * state.Source.Faces.size());
  state.Edges.reserve(3 * state.Source.Triangles.size());
  for (size_t k = 0; k <= segments; ++k) {
    state.Knots.push_back(static_cast<double>(k) / static_cast<double>(segments));
  }
  for (size_t at = 0; at < state.Source.Faces.size(); ++at) {
    ++state.Work.SourceFaceVisits;
    if (state.Source.Faces[at].Patch != state.Patch) { continue; }
    const auto &face = state.Source.Triangles[at];
    for (size_t corner = 0; corner < 3; ++corner) {
      const auto a = face[corner];
      const auto b = face[(corner + 1) % 3];
      const auto &p = state.Source.Priors[a].At;
      const auto term =
          ParameterAt(state.Source.Patches[state.Patch], {p.EastM, p.NorthM}, state.ToleranceM);
      if (!term) { return std::unexpected(term.error()); }
      state.Knots.push_back(term->T);
      state.Edges.push_back({.First = std::min(a, b), .Last = std::max(a, b)});
    }
  }
  std::ranges::sort(state.Knots);
  auto last = state.Knots.begin();
  for (const auto value : state.Knots) {
    if (last == state.Knots.begin() || value - *(last - 1) > kParameterTolerance) {
      *last++ = value;
    }
  }
  state.Knots.erase(last, state.Knots.end());
  std::ranges::sort(state.Edges);
  state.Edges.erase(std::ranges::unique(state.Edges).begin(), state.Edges.end());
  if (state.Knots.size() > state.Budget.StationsPerPatch ||
      state.Edges.size() > state.Budget.CacheEntries / state.Knots.size()) {
    return std::unexpected(RoadGroundError::CapacityExceeded);
  }
  state.Nodes.assign(state.Edges.size() * state.Knots.size(), std::numeric_limits<uint32_t>::max());
  return {};
}
}
