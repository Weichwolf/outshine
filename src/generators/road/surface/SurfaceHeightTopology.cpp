#include "SurfaceHeightTopologyInternal.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <numeric>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators::SurfaceHeights {
uint32_t Root(std::vector<uint32_t> &parents, uint32_t at) {
  while (parents[at] != at) {
    parents[at] = parents[parents[at]];
    at = parents[at];
  }
  return at;
}

uint32_t KeyIndex(std::span<const Key> keys, Key key) {
  const auto found = std::ranges::lower_bound(keys, key);
  assert(found != keys.end() && *found == key);
  return static_cast<uint32_t>(found - keys.begin());
}

namespace {
Checked<void> CollectKeys(const PlanarMesh &mesh, JoinState &state, size_t maximum) {
  for (uint32_t at = 0; at < mesh.Points.size(); ++at) {
    const auto point = mesh.Points[at];
    if (!std::isfinite(point[0]) || !std::isfinite(point[1])) {
      return std::unexpected(Error::InvalidInput);
    }
    auto bits = state.PointDomains[at];
    while (bits != 0) {
      if (state.Keys.size() == maximum ||
          state.Keys.size() == std::numeric_limits<uint32_t>::max()) {
        return std::unexpected(Error::CapacityExceeded);
      }
      state.Keys.push_back({static_cast<uint32_t>(std::countr_zero(bits)), at});
      bits &= bits - 1;
    }
  }
  std::ranges::sort(state.Keys);
  state.Parents.resize(state.Keys.size());
  std::ranges::iota(state.Parents, 0u);
  state.Work.SourceVertices = state.Keys.size();
  return {};
}

Checked<JoinState> Prepare(const PlanarArrangement &arrangement,
                           uint32_t domainCount,
                           SurfaceHeightTopologyBudget budget) {
  const auto &mesh = arrangement.Mesh;
  if (domainCount > std::numeric_limits<uint64_t>::digits ||
      mesh.Points.size() > budget.PlanarVertices ||
      mesh.Points.size() > std::numeric_limits<uint32_t>::max() ||
      mesh.Faces.size() > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(Error::CapacityExceeded);
  }
  if (mesh.Faces.size() != arrangement.FaceDomains.size()) {
    return std::unexpected(Error::InvalidInput);
  }
  const uint64_t valid = domainCount == std::numeric_limits<uint64_t>::digits
                             ? std::numeric_limits<uint64_t>::max()
                             : (uint64_t{1} << domainCount) - 1;
  JoinState state;
  state.PointDomains.resize(mesh.Points.size());
  state.FaceCounts.resize(domainCount);
  size_t count = 0;
  for (size_t at = 0; at < mesh.Faces.size(); ++at) {
    auto bits = arrangement.FaceDomains[at];
    if (bits == 0 || (bits & ~valid) != 0) { return std::unexpected(Error::InvalidInput); }
    const auto added = static_cast<size_t>(std::popcount(bits));
    if (added > budget.SourceFaces - count) { return std::unexpected(Error::CapacityExceeded); }
    count += added;
    for (const auto point : mesh.Faces[at]) {
      if (point >= mesh.Points.size()) { return std::unexpected(Error::InvalidInput); }
      state.PointDomains[point] |= bits;
    }
    while (bits != 0) {
      ++state.FaceCounts[static_cast<size_t>(std::countr_zero(bits))];
      bits &= bits - 1;
    }
  }
  if (auto result = CollectKeys(mesh, state, budget.SourceVertices); !result) {
    return std::unexpected(result.error());
  }
  return state;
}

SurfaceHeightTopology Pack(const PlanarArrangement &arrangement, JoinState &state) {
  SurfaceHeightTopology output;
  output.Work = state.Work;
  output.Sources.resize(state.FaceCounts.size());
  const size_t count = state.Keys.size() - state.Work.Joins;
  output.Points.reserve(count);
  output.PlanarVertices.reserve(count);
  output.PrimaryDomains.reserve(count);
  std::vector<uint32_t> variables(state.Keys.size());
  for (uint32_t at = 0; at < state.Keys.size(); ++at) {
    if (Root(state.Parents, at) != at) { continue; }
    variables[at] = static_cast<uint32_t>(output.Points.size());
    const auto key = state.Keys[at];
    output.PrimaryDomains.push_back(key[0]);
    output.PlanarVertices.push_back(key[1]);
    output.Points.push_back(arrangement.Mesh.Points[key[1]]);
  }
  for (uint32_t at = 0; at < state.Keys.size(); ++at) {
    variables[at] = variables[Root(state.Parents, at)];
  }
  output.PortVertices.reserve(state.PortKeys.size());
  for (const auto &keys : state.PortKeys) {
    auto &members = output.PortVertices.emplace_back();
    members.reserve(keys.size());
    for (const auto key : keys) { members.push_back(variables[Root(state.Parents, key)]); }
    std::ranges::sort(members);
    members.erase(std::ranges::unique(members).begin(), members.end());
  }
  for (size_t source = 0; source < output.Sources.size(); ++source) {
    auto &part = output.Sources[source];
    part.Faces.reserve(state.FaceCounts[source]);
    part.PlanarFaces.reserve(state.FaceCounts[source]);
  }
  for (uint32_t at = 0; at < arrangement.Mesh.Faces.size(); ++at) {
    auto bits = arrangement.FaceDomains[at];
    while (bits != 0) {
      const auto domain = static_cast<uint32_t>(std::countr_zero(bits));
      auto face = arrangement.Mesh.Faces[at];
      for (auto &point : face) { point = variables[KeyIndex(state.Keys, {domain, point})]; }
      output.Sources[domain].Faces.push_back(face);
      output.Sources[domain].PlanarFaces.push_back(at);
      bits &= bits - 1;
    }
  }
  return output;
}
}
}

namespace outshine::Generators {
std::expected<SurfaceHeightTopology, SurfaceHeightTopologyError>
BuildSurfaceHeightTopology(const PlanarArrangement &arrangement,
                           uint32_t domainCount,
                           std::span<const SurfaceHeightPort> ports,
                           double joinDistanceM,
                           SurfaceHeightTopologyBudget budget,
                           const std::stop_token &stop) {
  using namespace SurfaceHeights;
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  if (!std::isfinite(joinDistanceM) || joinDistanceM < 0) {
    return std::unexpected(Error::InvalidInput);
  }
  auto state = Prepare(arrangement, domainCount, budget);
  if (!state) { return std::unexpected(state.error()); }
  if (auto result = JoinPorts(arrangement, ports, joinDistanceM, *state, budget, stop); !result) {
    return std::unexpected(result.error());
  }
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  return Pack(arrangement, *state);
}
}
