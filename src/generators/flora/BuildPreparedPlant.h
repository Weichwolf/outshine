#ifndef OUTSHINE_GENERATORS_FLORA_BUILDPREPAREDPLANT_H
#define OUTSHINE_GENERATORS_FLORA_BUILDPREPAREDPLANT_H

#include "PlantPlan.h"
#include "PlantBladeSurface.h"
#include "PreparedPlant.h"
#include "scene/Geometry.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

namespace outshine::Generators {
struct PlantProductCounts {
  size_t Vertices = 0, Triangles = 0, LeafInstances = 0, TriangleReferences = 0;
};

struct PreparedPlantDemand {
  uint64_t Root = 0;
  size_t AxisSegments = 0, AxisSides = 0, BladeSegments = 0, BladeAcross = 0;
  PlantProductCounts Maximum;
};

[[nodiscard]] std::expected<PlantProductCounts, std::string_view> MeasurePreparedPlant(
    const PlantPlan &plan, std::span<const PlantBlade> blades, PreparedPlantDemand demand);
[[nodiscard]] std::expected<PreparedPlant, std::string_view>
BuildPreparedPlant(const PlantPlan &plan,
                   std::span<const PlantBlade> blades,
                   const std::array<uint8_t, 32> &identity,
                   const Geometry &materials,
                   PreparedPlantDemand demand);
}
#endif
