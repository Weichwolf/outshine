#include "MeshPairs.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace PatchSelection {
namespace {
constexpr uint32_t Missing = std::numeric_limits<uint32_t>::max();

struct Edge {
  uint32_t Low, High, Face, Slot;
};

struct Neighbor {
  uint32_t Face = Missing;
  uint32_t Opposite = Missing;
};

using Neighborhood = std::array<Neighbor, 3>;
using Normal = std::array<double, 3>;

Normal Direction(const Mesh &mesh, const Face &face) {
  Normal a{};
  Normal b{};
  for (size_t axis = 0; axis < 3; ++axis) {
    a[axis] = static_cast<double>(mesh.Positions[face[1]][axis]) - mesh.Positions[face[0]][axis];
    b[axis] = static_cast<double>(mesh.Positions[face[2]][axis]) - mesh.Positions[face[0]][axis];
  }
  Normal result{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
  const double length = std::hypot(result[0], result[1], result[2]);
  for (auto &component : result) { component = length > 0 ? component / length : 0; }
  return result;
}

std::vector<Edge> Edges(const Mesh &mesh, Costs &work) {
  std::vector<Edge> edges;
  edges.reserve(mesh.Faces.size() * 3);
  for (size_t id = 0; id < mesh.Faces.size(); ++id) {
    const auto &face = mesh.Faces[id];
    if (face[0] == face[1] || face[1] == face[2] || face[2] == face[0]) { continue; }
    for (uint32_t slot = 0; slot < 3; ++slot) {
      const auto a = face[slot];
      const auto b = face[(slot + 1) % 3];
      edges.push_back({.Low = std::min(a, b),
                       .High = std::max(a, b),
                       .Face = static_cast<uint32_t>(id),
                       .Slot = slot});
    }
  }
  std::ranges::sort(edges, [&](const Edge &a, const Edge &b) {
    ++work.SortComparisons;
    return std::pair{a.Low, a.High} < std::pair{b.Low, b.High};
  });
  work.Edges = edges.size();
  work.ScratchCapacityBytes += edges.capacity() * sizeof(Edge);
  work.AllocationRequests += edges.capacity() > 0 ? 1u : 0u;
  return edges;
}

std::vector<Neighborhood> Neighbors(const Mesh &mesh, Costs &work) {
  const auto edges = Edges(mesh, work);
  std::vector<Neighborhood> neighbors(mesh.Faces.size());
  for (size_t begin = 0; begin < edges.size();) {
    size_t end = begin + 1;
    while (end < edges.size() && edges[end].Low == edges[begin].Low &&
           edges[end].High == edges[begin].High) {
      ++end;
    }
    if (end - begin == 2) {
      const auto &a = edges[begin];
      const auto &b = edges[begin + 1];
      const auto &fa = mesh.Faces[a.Face];
      const auto &fb = mesh.Faces[b.Face];
      if (fa[a.Slot] == fb[(b.Slot + 1) % 3] && fa[(a.Slot + 1) % 3] == fb[b.Slot] &&
          fa[(a.Slot + 2) % 3] != fb[(b.Slot + 2) % 3]) {
        neighbors[a.Face][a.Slot] = {.Face = b.Face, .Opposite = fb[(b.Slot + 2) % 3]};
        neighbors[b.Face][b.Slot] = {.Face = a.Face, .Opposite = fa[(a.Slot + 2) % 3]};
      } else {
        ++work.UnpairableEdgeGroups;
      }
    } else {
      ++work.UnpairableEdgeGroups;
    }
    begin = end;
  }
  work.ScratchCapacityBytes += neighbors.capacity() * sizeof(Neighborhood);
  work.AllocationRequests += neighbors.capacity() > 0 ? 1u : 0u;
  return neighbors;
}

uint32_t BestNeighbor(const Neighborhood &neighbors,
                      std::span<const uint8_t> used,
                      std::span<const Normal> normals,
                      size_t id,
                      Policy policy,
                      Costs &work) {
  uint32_t chosen = Missing;
  double best = -2;
  for (uint32_t slot = 0; slot < 3; ++slot) {
    const auto other = neighbors[slot].Face;
    if (other == Missing || used[other] != 0) { continue; }
    ++work.NeighborCandidates;
    const double score = policy == Policy::FirstEdge ? 0
                                                     : normals[id][0] * normals[other][0] +
                                                           normals[id][1] * normals[other][1] +
                                                           normals[id][2] * normals[other][2];
    if (score > best) {
      best = score;
      chosen = slot;
      if (policy == Policy::FirstEdge) { break; }
    }
  }
  return chosen;
}

void Match(const Mesh &mesh, Policy policy, Selection &selection) {
  const auto neighbors = Neighbors(mesh, selection.Work);
  std::vector<Normal> normals;
  if (policy == Policy::AlignedNormals) {
    normals.reserve(mesh.Faces.size());
    for (const auto &face : mesh.Faces) { normals.push_back(Direction(mesh, face)); }
  }
  std::vector<uint8_t> used(mesh.Faces.size());
  for (size_t id = 0; id < mesh.Faces.size(); ++id) {
    if (used[id] != 0) { continue; }
    const auto chosen = BestNeighbor(neighbors[id], used, normals, id, policy, selection.Work);
    if (chosen == Missing) { continue; }
    const auto &face = mesh.Faces[id];
    const auto &other = neighbors[id][chosen];
    selection.Pairs.push_back(
        {.Corners = {face[(chosen + 2) % 3], face[chosen], other.Opposite, face[(chosen + 1) % 3]},
         .Faces = {static_cast<uint32_t>(id), other.Face}});
    used[id] = used[other.Face] = 1;
  }
  selection.Work.ScratchCapacityBytes +=
      used.capacity() * sizeof(uint8_t) + normals.capacity() * sizeof(Normal);
  selection.Work.AllocationRequests +=
      (used.capacity() > 0 ? 1u : 0u) + (normals.capacity() > 0 ? 1u : 0u);
}
}

std::expected<Selection, std::string_view> Select(const Mesh &mesh, Policy policy) {
  if (mesh.Faces.size() > Missing / 3) { return std::unexpected("too many faces"); }
  for (const auto &point : mesh.Positions) {
    for (const auto component : point) {
      if (!std::isfinite(component)) { return std::unexpected("nonfinite source position"); }
    }
  }
  for (const auto &face : mesh.Faces) {
    for (const auto id : face) {
      if (id >= mesh.Positions.size()) { return std::unexpected("source index out of range"); }
    }
  }
  Selection selection;
  selection.Pairs.reserve(mesh.Faces.size() / 2);
  Match(mesh, policy, selection);
  selection.Work.ResultCapacityBytes = selection.Pairs.capacity() * sizeof(Pair);
  selection.Work.AllocationRequests += selection.Pairs.capacity() > 0 ? 1u : 0u;
  return selection;
}
}
