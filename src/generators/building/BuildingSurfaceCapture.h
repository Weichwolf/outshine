#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGSURFACECAPTURE_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGSURFACECAPTURE_H

#include "StructureMesher.h"
#include <scene/ProjectedErrorBudget.h>

namespace outshine::Generators {

struct BuildingScratch;

[[nodiscard]] std::expected<ProjectedStructureMesh, StructureMeshError>
CaptureBuildingSurfaces(std::span<const StructurePlan> plans,
                        const Vec3 &eye,
                        ProjectedErrorBudget projection,
                        BuildingScratch &scratch);

}
#endif
