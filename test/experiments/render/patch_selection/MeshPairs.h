#ifndef OUTSHINE_EXPERIMENT_MESH_PAIRS_H
#define OUTSHINE_EXPERIMENT_MESH_PAIRS_H

#include <array>
#include <cstdint>
#include <expected>
#include <string_view>
#include <vector>

namespace PatchSelection {
using Position = std::array<float, 3>;
using Face = std::array<uint32_t, 3>;

struct Mesh {
  std::vector<Position> Positions;
  std::vector<Face> Faces;
};

struct Pair {
  std::array<uint32_t, 4> Corners;
  std::array<uint32_t, 2> Faces;
};
enum class Policy { FirstEdge, AlignedNormals };

struct Costs {
  uint64_t SortComparisons = 0;
  uint64_t Edges = 0;
  uint64_t NeighborCandidates = 0;
  uint64_t UnpairableEdgeGroups = 0;
  uint64_t ScratchCapacityBytes = 0;
  uint64_t ResultCapacityBytes = 0;
  uint32_t AllocationRequests = 0;
};

struct Selection {
  std::vector<Pair> Pairs;
  Costs Work;
};

[[nodiscard]] std::expected<Selection, std::string_view> Select(const Mesh &mesh, Policy policy);
}
#endif
