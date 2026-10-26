#include "PlanarStationMeshInternal.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>

namespace outshine::PlanarStations {
namespace {
struct Partition {
  std::array<PlanarFace, 4> Triangles;
  size_t Count = 1;
};

struct EdgeCut {
  uint32_t First = 0, Second = 0, Node = 0;
};

bool SplitEdge(Partition &partition, EdgeCut edge) {
  for (size_t at = 0; at < partition.Count; ++at) {
    for (size_t corner = 0; corner < PlanarFace{}.size(); ++corner) {
      if (partition.Triangles[at][corner] != edge.First ||
          partition.Triangles[at][(corner + 1) % PlanarFace{}.size()] != edge.Second) {
        continue;
      }
      const auto opposite = partition.Triangles[at][(corner + 2) % PlanarFace{}.size()];
      std::move_backward(partition.Triangles.begin() + at + 1,
                         partition.Triangles.begin() + partition.Count,
                         partition.Triangles.begin() + partition.Count + 1);
      partition.Triangles[at] = {edge.First, edge.Node, opposite};
      partition.Triangles[at + 1] = {edge.Node, edge.Second, opposite};
      ++partition.Count;
      return true;
    }
  }
  return false;
}
}

Checked<void> Conform(CutState &state, PlanarFace face, uint32_t parent) {
  Partition partition{.Triangles = {face}};
  for (size_t side = 0; side < face.size(); ++side) {
    const auto a = face[side];
    const auto b = face[(side + 1) % face.size()];
    const uint64_t edge = (uint64_t{std::min(a, b)} << 32) | std::max(a, b);
    const auto found = state.Edges.find(edge);
    if (found == state.Edges.end()) { continue; }
    if (!SplitEdge(partition, {.First = a, .Second = b, .Node = found->second})) {
      return std::unexpected(Error::InvalidInput);
    }
  }
  for (size_t at = 0; at < partition.Count; ++at) {
    const auto emitted = Emit(state, partition.Triangles[at], parent);
    if (!emitted) { return emitted; }
  }
  return {};
}
}
