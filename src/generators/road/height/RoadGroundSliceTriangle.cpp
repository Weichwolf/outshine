#include "RoadGroundMeshInternal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>

namespace outshine::Generators::RoadGround {
namespace {
MeshResult<SlicePolygon> Clip(SliceState &state,
                              const PlanarFace &original,
                              const SlicePolygon &polygon,
                              size_t knot,
                              bool above) {
  SlicePolygon output{};
  const double level = state.Knots[knot];
  const auto append = [&](SliceNode node) {
    if (output.Count != 0 && output.Nodes[output.Count - 1].Vertex == node.Vertex) { return true; }
    if (output.Count == output.Nodes.size()) { return false; }
    output.Nodes[output.Count++] = node;
    return true;
  };
  for (size_t at = 0; at < polygon.Count; ++at) {
    const auto &a = polygon.Nodes[at];
    const auto &b = polygon.Nodes[(at + 1) % polygon.Count];
    const bool inside = above ? a.Parameter >= level : a.Parameter <= level;
    if (inside && !append(a)) { return std::unexpected(RoadGroundError::InvalidGeometry); }
    if (inside != (above ? b.Parameter >= level : b.Parameter <= level)) {
      const auto crossing = Crossing(state, original, a, b, knot);
      if (!crossing) { return std::unexpected(crossing.error()); }
      if (!append(*crossing)) { return std::unexpected(RoadGroundError::InvalidGeometry); }
    }
  }
  if (output.Count > 1 && output.Nodes[0].Vertex == output.Nodes[output.Count - 1].Vertex) {
    --output.Count;
  }
  return output;
}

MeshResult<void> Emit(SliceState &state, PlanarFace face, size_t parent) {
  if (face[0] == face[1] || face[0] == face[2] || face[1] == face[2]) { return {}; }
  const auto &p = state.Mesh.Points;
  const double area = MeshArea(p[face[0]], p[face[1]], p[face[2]]);
  if (!(area > 0)) {
    if (area == 0) { return {}; }
    return std::unexpected(RoadGroundError::InvalidGeometry);
  }
  if (state.Mesh.Faces.size() >= state.Budget.Faces) {
    return std::unexpected(RoadGroundError::CapacityExceeded);
  }
  state.Mesh.Faces.push_back(face);
  state.Mesh.FaceParents.push_back(static_cast<uint32_t>(parent));
  return {};
}
}

MeshResult<void> SliceTriangle(SliceState &state, size_t original) {
  const auto &face = state.Source.Triangles[original];
  SlicePolygon polygon{};
  polygon.Count = 3;
  for (size_t corner = 0; corner < 3; ++corner) {
    const auto &point = state.Source.Priors[face[corner]].At;
    const auto term = ParameterAt(
        state.Source.Patches[state.Patch], {point.EastM, point.NorthM}, state.ToleranceM);
    if (!term) { return std::unexpected(term.error()); }
    polygon.Nodes[corner] = {.Vertex = face[corner], .Parameter = term->T, .Support = 1u << corner};
  }
  const auto [low, high] = std::minmax(
      {polygon.Nodes[0].Parameter, polygon.Nodes[1].Parameter, polygon.Nodes[2].Parameter});
  if (low == high) { return Emit(state, face, original); }
  size_t first =
      static_cast<size_t>(std::ranges::upper_bound(state.Knots, low) - state.Knots.begin());
  if (first > 0) { --first; }
  const size_t last = std::min(
      state.Knots.size() - 1,
      static_cast<size_t>(std::ranges::lower_bound(state.Knots, high) - state.Knots.begin()));
  for (size_t interval = first; interval < last; ++interval) {
    ++state.Work.IntervalVisits;
    auto clipped = Clip(state, face, polygon, interval, true);
    if (!clipped) { return std::unexpected(clipped.error()); }
    clipped = Clip(state, face, *clipped, interval + 1, false);
    if (!clipped) { return std::unexpected(clipped.error()); }
    for (size_t at = 1; at + 1 < clipped->Count; ++at) {
      auto emitted =
          Emit(state,
               {clipped->Nodes[0].Vertex, clipped->Nodes[at].Vertex, clipped->Nodes[at + 1].Vertex},
               original);
      if (!emitted) { return emitted; }
    }
  }
  return {};
}
}
