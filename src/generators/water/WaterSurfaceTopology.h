#ifndef OUTSHINE_GENERATORS_WATER_WATERSURFACETOPOLOGY_H
#define OUTSHINE_GENERATORS_WATER_WATERSURFACETOPOLOGY_H

#include "WaterAsset.h"
#include "TangentFrame.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::Generators {
struct WaterSurfaceTopology {
  LongitudeLatitude Anchor;
  std::vector<std::array<double, 2>> VerticesXY;
  std::vector<WaterSurfaceMesh::Face> Faces;
  std::vector<uint32_t> FaceSourceOffsets;
  std::vector<uint32_t> FaceSources;
  double AreaM2 = 0;
  size_t Intersections = 0;
};

struct WaterTopologyBudget {
  static constexpr double DefaultBoundarySpacingM = 20;
  static constexpr uint32_t DefaultAssociationBudget = 100000;
  double BoundarySpacingM = DefaultBoundarySpacingM;
  uint32_t Sources = 4096;
  uint32_t SourceCoordinates = 262144;
  uint32_t Vertices = 8192;
  uint32_t Faces = 16384;
  uint32_t Intersections = DefaultAssociationBudget;
  uint32_t Memberships = DefaultAssociationBudget;
};

enum class WaterTopologyError {
  InvalidInput,
  InvalidGeometry,
  CapacityExceeded,
  LibraryFailure,
  Canceled
};

[[nodiscard]] std::expected<WaterSurfaceTopology, WaterTopologyError>
BuildWaterSurfaceTopology(const WaterAsset &water,
                          std::span<const uint32_t> admittedSurfaces,
                          const TangentFrame &frame,
                          WaterTopologyBudget budget = {},
                          const std::stop_token &stop = {});

[[nodiscard]] std::expected<std::vector<WaterSurfaceTopology>, WaterTopologyError>
SplitWaterSurfaceComponents(WaterSurfaceTopology topology, const std::stop_token &stop = {});
}
#endif
