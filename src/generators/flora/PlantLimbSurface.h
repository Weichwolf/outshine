#ifndef OUTSHINE_GENERATORS_FLORA_PLANTLIMBSURFACE_H
#define OUTSHINE_GENERATORS_FLORA_PLANTLIMBSURFACE_H

#include "PlantLimbPlan.h"
#include "PlantSurface.h"
#include <cstddef>
#include <expected>
#include <string_view>

namespace outshine::Generators {
struct PlantLimbSurfaceDemand {
  size_t Along = 0, Sides = 0, MaximumVertices = 0, MaximumTriangles = 0;
};

[[nodiscard]] std::expected<PlantSurfaceSize, std::string_view>
MeasurePlantLimbSurface(const PlantLimbPlan &plan, PlantLimbSurfaceDemand demand) noexcept;
[[nodiscard]] std::expected<PlantSurface, std::string_view>
BuildPlantLimbSurface(const PlantLimbPlan &plan, PlantLimbSurfaceDemand demand);
}
#endif
