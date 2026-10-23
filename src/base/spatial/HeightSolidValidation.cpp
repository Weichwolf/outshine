#include "HeightSolidInternal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <stop_token>
#include <unordered_map>
#include <vector>

namespace outshine::HeightSolids {
namespace {
constexpr uint32_t kAbsent = std::numeric_limits<uint32_t>::max();

bool Finite(const auto &point) {
  return std::ranges::all_of(point, [](double value) { return std::isfinite(value); });
}

double Area(const HeightSolidInput &input, PlanarFace face) {
  const auto a = input.Points[face[0]];
  const auto b = input.Points[face[1]];
  const auto c = input.Points[face[2]];
  return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
}

Checked<void> SurfaceFace(const HeightSolidInput &input, size_t at) {
  const auto face = input.Faces[at];
  for (size_t corner = 0; corner < face.size(); ++corner) {
    if (face[corner] >= input.Points.size()) { return std::unexpected(Error::InvalidInput); }
    const double height = input.TopHeights[at][corner];
    if (!std::isfinite(height) || height <= input.BottomHeights[face[corner]]) {
      return std::unexpected(Error::InvalidInput);
    }
  }
  const double area = Area(input, face);
  if (!std::isfinite(area) || area <= 0) { return std::unexpected(Error::InvalidInput); }
  return {};
}

bool Nondegenerate(const HeightSolid &body, PlanarFace face) {
  const auto &a = body.Points[face[0]];
  const auto &b = body.Points[face[1]];
  const auto &c = body.Points[face[2]];
  const std::array u{b[0] - a[0], b[1] - a[1], b[2] - a[2]};
  const std::array v{c[0] - a[0], c[1] - a[1], c[2] - a[2]};
  const std::array cross{
      u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
  return Finite(cross) && std::ranges::any_of(cross, [](double value) { return value != 0; });
}

struct Topology {
  std::vector<uint32_t> Twins, Starts, Degrees;
};

Checked<void> PairEdges(State &state, Topology &topology) {
  std::unordered_map<uint64_t, uint32_t> first;
  first.reserve(state.Output.Faces.size() * 3);
  for (uint32_t at = 0; at < state.Output.Faces.size(); ++at) {
    const auto face = state.Output.Faces[at];
    if (!Nondegenerate(state.Output, face)) { return std::unexpected(Error::Unrepresentable); }
    for (uint32_t corner = 0; corner < face.size(); ++corner) {
      if (auto valid = state.VisitEdge(); !valid) { return valid; }
      const auto a = face[corner];
      const auto b = face[(corner + 1) % static_cast<uint32_t>(face.size())];
      const uint64_t key = (uint64_t{std::min(a, b)} << 32) | std::max(a, b);
      const auto edge = at * 3 + corner;
      topology.Starts[a] = edge;
      ++topology.Degrees[a];
      const auto [found, inserted] = first.try_emplace(key, edge);
      if (inserted) { continue; }
      const auto previous = found->second;
      const auto old = state.Output.Faces[previous / 3];
      if (topology.Twins[previous] != kAbsent || old[previous % 3] != b) {
        return std::unexpected(Error::NonManifold);
      }
      topology.Twins[previous] = edge;
      topology.Twins[edge] = previous;
    }
  }
  if (std::ranges::find(topology.Twins, kAbsent) != topology.Twins.end()) {
    return std::unexpected(Error::NonManifold);
  }
  return {};
}

Checked<void> VertexFans(const State &state, const Topology &topology) {
  const auto &twins = topology.Twins;
  const auto &starts = topology.Starts;
  const auto &degrees = topology.Degrees;
  for (size_t vertex = 0; vertex < starts.size(); ++vertex) {
    if (auto valid = state.Check(); !valid) { return valid; }
    if (starts[vertex] == kAbsent) { return std::unexpected(Error::NonManifold); }
    uint32_t current = starts[vertex];
    uint32_t visited = 0;
    do {
      const auto twin = twins[current];
      current = twin - twin % 3 + (twin % 3 + 1) % 3;
      ++visited;
    } while (current != starts[vertex] && visited <= degrees[vertex]);
    if (visited != degrees[vertex]) { return std::unexpected(Error::NonManifold); }
  }
  return {};
}
}

Checked<void> ValidateInput(const HeightSolidInput &input,
                            HeightSolidBudget budget,
                            const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  constexpr size_t indexLimit = std::numeric_limits<uint32_t>::max();
  if (input.Points.size() > budget.Points || input.Points.size() > indexLimit ||
      input.Faces.size() > budget.SurfaceFaces || input.Faces.size() > budget.Faces / 2 ||
      input.Faces.size() > indexLimit / 6) {
    return std::unexpected(Error::CapacityExceeded);
  }
  if (input.TopHeights.size() != input.Faces.size() ||
      input.BottomHeights.size() != input.Points.size()) {
    return std::unexpected(Error::InvalidInput);
  }
  for (size_t at = 0; at < input.Points.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (!Finite(input.Points[at]) || !std::isfinite(input.BottomHeights[at])) {
      return std::unexpected(Error::InvalidInput);
    }
  }
  for (size_t at = 0; at < input.Faces.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (auto valid = SurfaceFace(input, at); !valid) { return valid; }
  }
  return {};
}

Checked<void> ValidateBody(State &state) {
  Topology topology{.Twins = std::vector<uint32_t>(state.Output.Faces.size() * 3, kAbsent),
                    .Starts = std::vector<uint32_t>(state.Output.Points.size(), kAbsent),
                    .Degrees = std::vector<uint32_t>(state.Output.Points.size())};
  if (auto valid = PairEdges(state, topology); !valid) { return valid; }
  if (auto valid = VertexFans(state, topology); !valid) { return valid; }
  double volume = 0;
  double correction = 0;
  for (size_t at = 0; at < state.Input.Faces.size(); ++at) {
    const auto face = state.Input.Faces[at];
    double thickness = 0;
    for (size_t corner = 0; corner < face.size(); ++corner) {
      thickness += state.Input.TopHeights[at][corner] - state.Input.BottomHeights[face[corner]];
    }
    const double value = Area(state.Input, face) * thickness / 6 - correction;
    const double total = volume + value;
    correction = (total - volume) - value;
    volume = total;
  }
  if (!std::isfinite(volume) || volume <= 0) { return std::unexpected(Error::Unrepresentable); }
  state.Output.VolumeM3 = volume;
  return state.Check();
}
}
