#ifndef OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADGROUNDMESHINTERNAL_H
#define OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADGROUNDMESHINTERNAL_H

#include "RoadGroundMesh.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::Generators::RoadGround {
template <typename T> using MeshResult = std::expected<T, RoadGroundError>;

struct Parameter {
  double U = 0;
  double D = 1;
  double T = 0;
};

struct SliceEdge {
  uint32_t First = 0;
  uint32_t Last = 0;
  auto operator<=>(const SliceEdge &) const = default;
};

struct SliceNode {
  uint32_t Vertex = 0;
  double Parameter = 0;
  unsigned Support = 0;
};

struct SlicePolygon {
  std::array<SliceNode, 8> Nodes;
  size_t Count = 0;
};

struct SliceState {
  const RoadGroundField &Source;
  PlanarMesh &Mesh;
  RoadGroundMeshWork &Work;
  RoadGroundMeshBudget Budget;
  size_t Patch = 0;
  double ToleranceM = 0;
  std::vector<double> Knots;
  std::vector<SliceEdge> Edges;
  std::vector<uint32_t> Nodes;
};

[[nodiscard]] double MeshArea(PlanarPoint a, PlanarPoint b, PlanarPoint c) noexcept;
[[nodiscard]] MeshResult<Parameter>
ParameterAt(const RoadPortField::Patch &patch, PlanarPoint point, double toleranceM) noexcept;
[[nodiscard]] MeshResult<void> PreparePatch(SliceState &state, size_t segments);
[[nodiscard]] MeshResult<SliceNode>
Crossing(SliceState &state, const PlanarFace &original, SliceNode a, SliceNode b, size_t knot);
[[nodiscard]] MeshResult<void> SliceTriangle(SliceState &state, size_t original);
[[nodiscard]] MeshResult<void> AdmitMeshSource(const RoadGroundField &source,
                                               RoadGroundMeshBudget budget);
[[nodiscard]] MeshResult<PlanarMesh> SliceMesh(const RoadGroundField &source,
                                               std::span<const size_t> segments,
                                               RoadGroundMeshBudget budget,
                                               RoadGroundMeshWork &work,
                                               const std::stop_token &stop);
[[nodiscard]] MeshResult<void> CheckMeshCoverage(const RoadGroundField &source,
                                                 const PlanarMesh &mesh,
                                                 RoadGroundMeshWork &work,
                                                 const std::stop_token &stop);
}
#endif
