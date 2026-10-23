#ifndef OUTSHINE_BASE_SPATIAL_PLANARMESHINTERNAL_H
#define OUTSHINE_BASE_SPATIAL_PLANARMESHINTERNAL_H

#include "PlanarMesh.h"
#include "PlanHierarchy.h"

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::PlanarMeshDetail {
using Edge = std::array<uint32_t, 2>;

struct Split {
  uint32_t EdgeIndex = 0;
  uint32_t Point = 0;
  double Station = 0.0;
};

[[nodiscard]] Box PointBox(PlanarPoint point, double reach);
[[nodiscard]] double Area(PlanarPoint a, PlanarPoint b, PlanarPoint c);
[[nodiscard]] std::expected<void, PlanarMeshError> Weld(PlanarMesh &mesh,
                                                        std::span<const PlanarPoint> points,
                                                        double distance,
                                                        PlanarMeshBudget budget,
                                                        const std::stop_token &stop);
[[nodiscard]] std::expected<std::vector<Split>, PlanarMeshError>
FindSplits(PlanarMesh &mesh,
           std::span<const Edge> edges,
           PlanarMeshBudget budget,
           const std::stop_token &stop);
}
#endif
