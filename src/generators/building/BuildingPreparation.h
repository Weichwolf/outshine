#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGPREPARATION_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGPREPARATION_H

#include "BuildingShape.h"

#include <expected>
#include <span>

namespace outshine::Generators {

[[nodiscard]] std::expected<std::span<BuildingShape>, StructureMeshError>
PrepareBuildingShapes(const StructurePlan &plan, BuildingScratch &scratch);

[[nodiscard]] double BuildingBottomM(const BuildingShape &shape, double minimumHeightM) noexcept;

}
#endif
