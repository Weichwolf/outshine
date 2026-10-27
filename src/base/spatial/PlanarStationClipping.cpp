#include "PlanarStationMeshInternal.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <unordered_map>
#include <utility>
#include <vector>

namespace outshine::PlanarStations {
namespace {
constexpr size_t kMaximumCuts = 32;

struct Polygon {
  std::array<uint32_t, 3 + kMaximumCuts> Vertices{};
  size_t Count = 0;

  [[nodiscard]] Checked<void> Append(uint32_t vertex) {
    if (Count > 0 && Vertices[Count - 1] == vertex) { return {}; }
    if (Count == Vertices.size()) { return std::unexpected(Error::CapacityExceeded); }
    Vertices[Count++] = vertex;
    return {};
  }
};

Checked<Polygon> Clip(CutState &state, const Polygon &polygon, bool above) {
  Polygon result;
  const auto inside = [&](uint32_t vertex) {
    const auto station = state.Output.Stations[state.Axis][vertex];
    return above ? station >= state.Station : station <= state.Station;
  };
  for (size_t at = 0; at < polygon.Count; ++at) {
    const auto a = polygon.Vertices[at];
    const auto b = polygon.Vertices[(at + 1) % polygon.Count];
    const bool included = inside(a);
    if (included) {
      if (auto added = result.Append(a); !added) { return std::unexpected(added.error()); }
    }
    if (included != inside(b)) {
      const auto crossing = Crossing(state, a, b);
      if (!crossing) { return std::unexpected(crossing.error()); }
      if (auto added = result.Append(*crossing); !added) { return std::unexpected(added.error()); }
    }
  }
  if (result.Count > 1 && result.Vertices[0] == result.Vertices[result.Count - 1]) {
    --result.Count;
  }
  return result;
}

using Intersections = std::unordered_map<uint64_t, uint32_t>;

Checked<void> Face(CutState &state,
                   PlanarFace face,
                   uint32_t parent,
                   std::span<const PlanarStationAxis> axes,
                   std::span<const bool> above,
                   std::span<Intersections> intersections,
                   const std::stop_token &stop) {
  Polygon polygon{.Vertices = {face[0], face[1], face[2]}, .Count = 3};
  size_t cut = 0;
  for (size_t axis = 0; axis < axes.size(); ++axis) {
    const auto &descriptor = axes[axis];
    if (descriptor.InitialFaces && !std::ranges::binary_search(*descriptor.InitialFaces, parent)) {
      cut += descriptor.Cuts.size();
      continue;
    }
    state.Axis = axis;
    for (const auto station : descriptor.Cuts) {
      if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
      if (state.Output.Work.FaceVisits >= state.Budget.FaceVisits) {
        return std::unexpected(Error::CapacityExceeded);
      }
      ++state.Output.Work.FaceVisits;
      state.Station = station;
      state.Edges.swap(intersections[cut]);
      auto clipped = Clip(state, polygon, above[axis]);
      if (!clipped) { return std::unexpected(clipped.error()); }
      polygon = *clipped;
      state.Edges.swap(intersections[cut++]);
    }
    if (polygon.Count < 3) { return {}; }
  }
  for (size_t at = 1; at + 1 < polygon.Count; ++at) {
    if (auto emitted = Emit(
            state, {polygon.Vertices[0], polygon.Vertices[at], polygon.Vertices[at + 1]}, parent);
        !emitted) {
      return emitted;
    }
  }
  return {};
}
}
}

namespace outshine {
std::expected<PlanarStationMesh, PlanarMeshError>
ClipPlanarStations(std::span<const PlanarPoint> points,
                   std::span<const PlanarFace> faces,
                   std::span<const PlanarStationAxis> axes,
                   std::span<const bool> above,
                   PlanarStationBudget budget,
                   const std::stop_token &stop) {
  using namespace PlanarStations;
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  if (above.size() != axes.size()) { return std::unexpected(Error::InvalidInput); }
  budget.Cuts = std::min(budget.Cuts, kMaximumCuts);
  if (auto valid = Validate(points, faces, axes, budget, stop); !valid) {
    return std::unexpected(valid.error());
  }
  auto output = Prepare(points, faces, axes);
  if (!output) { return output; }
  std::vector<SourceEdge> roots;
  roots.reserve(points.size());
  for (uint32_t point = 0; point < points.size(); ++point) { roots.push_back({point, point}); }
  CutState state{.Output = *output,
                 .Axis = 0,
                 .Station = 0,
                 .Budget = budget,
                 .Roots = roots,
                 .Edges = {},
                 .Faces = {},
                 .Parents = {},
                 .Remaining = {}};
  size_t cuts = 0;
  for (const auto &axis : axes) { cuts += axis.Cuts.size(); }
  std::vector<Intersections> intersections(cuts);
  for (uint32_t at = 0; at < faces.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (auto clipped = Face(state, faces[at], at, axes, above, intersections, stop); !clipped) {
      return std::unexpected(clipped.error());
    }
  }
  output->Mesh.Faces = std::move(state.Faces);
  output->Mesh.FaceParents = std::move(state.Parents);
  output->SourceEdges = std::move(roots);
  return output;
}
}
