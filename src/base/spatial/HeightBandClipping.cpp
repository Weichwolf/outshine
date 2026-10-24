#include "HeightBandInternal.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace outshine::HeightBands {
Checked<void> Polygon::Append(uint32_t vertex) {
  const auto used = std::span(Vertices).first(Count);
  if (std::ranges::find(used, vertex) != used.end()) { return {}; }
  if (Count == Vertices.size()) { return std::unexpected(Error::Unrepresentable); }
  Vertices[Count++] = vertex;
  return {};
}

Checked<Polygon> Clip(State &state, const Polygon &polygon, uint32_t level, HalfSpace half) {
  Polygon result;
  const double height = state.Input.Levels[level];
  for (size_t at = 0; at < polygon.Count; ++at) {
    if (state.Output.Work.EdgeVisits >= state.Budget.EdgeVisits) {
      return std::unexpected(Error::CapacityExceeded);
    }
    ++state.Output.Work.EdgeVisits;
    const auto a = polygon.Vertices[at];
    const auto b = polygon.Vertices[(at + 1) % polygon.Count];
    const auto za = state.Output.Points[a][2];
    const auto zb = state.Output.Points[b][2];
    const bool insideA = half == HalfSpace::Above ? za >= height : za <= height;
    const bool insideB = half == HalfSpace::Above ? zb >= height : zb <= height;
    if (insideA) {
      if (auto added = result.Append(a); !added) { return std::unexpected(added.error()); }
    }
    if (insideA != insideB) {
      const auto point = Crossing(state, {a, b}, level);
      if (!point) { return std::unexpected(point.error()); }
      if (auto added = result.Append(*point); !added) { return std::unexpected(added.error()); }
    }
  }
  return result;
}

Checked<void> Emit(State &state, const Polygon &polygon, FaceOwner owner) {
  for (size_t corner = 1; corner + 1 < polygon.Count; ++corner) {
    const PlanarFace face{
        polygon.Vertices[0], polygon.Vertices[corner], polygon.Vertices[corner + 1]};
    const auto &a = state.Output.Points[face[0]];
    const auto &b = state.Output.Points[face[1]];
    const auto &c = state.Output.Points[face[2]];
    const double area = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
    if (area == 0) {
      ++state.Output.Work.RemovedZeroFaces;
      continue;
    }
    if (!std::isfinite(area) || area < 0) { return std::unexpected(Error::Unrepresentable); }
    if (state.Output.Faces.size() >= state.Budget.Faces) {
      return std::unexpected(Error::CapacityExceeded);
    }
    state.Output.Faces.push_back(face);
    state.Output.FaceParents.push_back(owner.Parent);
    state.Output.LevelIndices.push_back(owner.Level);
  }
  return {};
}
}
