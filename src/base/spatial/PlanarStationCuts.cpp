#include "PlanarStationMeshInternal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <stop_token>
#include <unordered_map>
#include <utility>
#include <vector>

namespace outshine::PlanarStations {
namespace {
std::optional<SourceEdge> OriginalEdge(const CutState &state, uint32_t a, uint32_t b) {
  if (state.Roots.empty()) { return {}; }
  const auto &first = state.Roots[a];
  const auto &second = state.Roots[b];
  const uint32_t low = std::min({first[0], first[1], second[0], second[1]});
  const uint32_t high = std::max({first[0], first[1], second[0], second[1]});
  if (high == std::numeric_limits<uint32_t>::max() || low == high) { return {}; }
  for (const auto point : std::array{first[0], first[1], second[0], second[1]}) {
    if (point != low && point != high) { return {}; }
  }
  return SourceEdge{low, high};
}

}

Checked<uint32_t> Crossing(CutState &state, uint32_t a, uint32_t b) {
  auto &mesh = state.Output.Mesh;
  auto &stations = state.Output.Stations[state.Axis];
  if (stations[a] == state.Station) { return a; }
  if (stations[b] == state.Station) { return b; }
  if (a > b) { std::swap(a, b); }
  const uint64_t current = (uint64_t{a} << 32) | b;
  const auto original = OriginalEdge(state, a, b);
  if (original) {
    a = (*original)[0];
    b = (*original)[1];
  }
  const uint64_t edge = (uint64_t{a} << 32) | b;
  if (const auto found = state.Edges.find(edge); found != state.Edges.end()) {
    const auto index = found->second;
    if (current != edge) { state.Edges.emplace(current, index); }
    return index;
  }
  if (mesh.Points.size() >= state.Budget.Vertices ||
      mesh.Points.size() >= std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(Error::CapacityExceeded);
  }
  const double fraction = (state.Station - stations[a]) / (stations[b] - stations[a]);
  const auto start = mesh.Points[a];
  const auto end = mesh.Points[b];
  const PlanarPoint point{start[0] + fraction * (end[0] - start[0]),
                          start[1] + fraction * (end[1] - start[1])};
  if (!std::isfinite(fraction) || fraction <= 0 || fraction >= 1 || !std::isfinite(point[0]) ||
      !std::isfinite(point[1])) {
    return std::unexpected(Error::InvalidInput);
  }
  for (auto &axis : state.Output.Stations) {
    const double station = axis[a] + fraction * (axis[b] - axis[a]);
    if (!std::isfinite(station)) { return std::unexpected(Error::InvalidInput); }
    axis.push_back(station);
  }
  stations.back() = state.Station;
  const auto index = static_cast<uint32_t>(mesh.Points.size());
  mesh.Points.push_back(point);
  if (!state.Roots.empty()) {
    constexpr auto absent = std::numeric_limits<uint32_t>::max();
    state.Roots.push_back(original.value_or(SourceEdge{absent, absent}));
  }
  state.Edges.emplace(edge, index);
  if (current != edge) { state.Edges.emplace(current, index); }
  ++state.Output.Work.EdgeIntersections;
  return index;
}

Checked<void> Emit(CutState &state, PlanarFace face, uint32_t parent) {
  const auto &points = state.Output.Mesh.Points;
  const double area = Area(points[face[0]], points[face[1]], points[face[2]]);
  if (!std::isfinite(area)) { return std::unexpected(Error::InvalidInput); }
  if (area == 0) { return {}; }
  if (area < 0) { std::ranges::reverse(face); }
  if (state.Faces.size() >= state.Budget.Faces ||
      state.Faces.size() >= std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(Error::CapacityExceeded);
  }
  state.Faces.push_back(face);
  state.Parents.push_back(parent);
  return {};
}

namespace {
Checked<void> Visit(CutState &state, const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  if (state.Output.Work.FaceVisits == state.Budget.FaceVisits) {
    return std::unexpected(Error::CapacityExceeded);
  }
  ++state.Output.Work.FaceVisits;
  return {};
}

Checked<void> Clip(CutState &state, PlanarFace face, uint32_t parent, bool above) {
  std::array<uint32_t, 4> polygon{};
  size_t count = 0;
  const auto inside = [&](uint32_t vertex) {
    const double station = state.Output.Stations[state.Axis][vertex];
    return above ? station >= state.Station : station <= state.Station;
  };
  const auto append = [&](uint32_t vertex) {
    if (count == 0 || polygon[count - 1] != vertex) { polygon[count++] = vertex; }
  };
  for (size_t at = 0; at < face.size(); ++at) {
    const auto a = face[at];
    const auto b = face[(at + 1) % face.size()];
    const bool insideA = inside(a);
    if (insideA) { append(a); }
    if (insideA != inside(b)) {
      const auto intersection = Crossing(state, a, b);
      if (!intersection) { return std::unexpected(intersection.error()); }
      append(*intersection);
    }
  }
  if (count > 1 && polygon[0] == polygon[count - 1]) { --count; }
  for (size_t at = 1; at + 1 < count; ++at) {
    if (auto result = Emit(state, {polygon[0], polygon[at], polygon[at + 1]}, parent); !result) {
      return result;
    }
  }
  return {};
}
}

Checked<void> Cut(PlanarStationMesh &output,
                  size_t axis,
                  double station,
                  std::optional<std::span<const uint8_t>> selectedParents,
                  std::vector<SourceEdge> &roots,
                  PlanarStationBudget budget,
                  const std::stop_token &stop) {
  CutState state{.Output = output,
                 .Axis = axis,
                 .Station = station,
                 .Budget = budget,
                 .Roots = roots,
                 .Edges = {},
                 .Faces = {},
                 .Parents = {},
                 .Remaining = {}};
  state.Faces.reserve(output.Mesh.Faces.size());
  state.Parents.reserve(output.Mesh.Faces.size());
  for (size_t at = 0; at < output.Mesh.Faces.size(); ++at) {
    if (const auto visited = Visit(state, stop); !visited) { return visited; }
    const auto face = output.Mesh.Faces[at];
    const auto parent = output.Mesh.FaceParents[at];
    if (selectedParents && (*selectedParents)[parent] == 0) {
      state.Remaining.push_back(static_cast<uint32_t>(at));
      continue;
    }
    const auto &values = output.Stations[axis];
    const auto [low, high] = std::minmax({values[face[0]], values[face[1]], values[face[2]]});
    if (low >= station || high <= station) {
      if (auto result = Emit(state, face, parent); !result) { return result; }
    } else {
      if (auto result = Clip(state, face, parent, false); !result) { return result; }
      if (auto result = Clip(state, face, parent, true); !result) { return result; }
    }
  }
  for (const auto at : state.Remaining) {
    if (const auto visited = Visit(state, stop); !visited) { return visited; }
    const auto conformed = Conform(state, output.Mesh.Faces[at], output.Mesh.FaceParents[at]);
    if (!conformed) { return conformed; }
  }
  output.Mesh.Faces = std::move(state.Faces);
  output.Mesh.FaceParents = std::move(state.Parents);
  return {};
}
}
