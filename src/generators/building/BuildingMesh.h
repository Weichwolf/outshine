#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGMESH_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGMESH_H

#include "FacadeUv.h"
#include "StructureMesher.h"

namespace outshine::Generators {

class BuildingMesh : public StructureMesher {
public:
  [[nodiscard]] bool Mesh(const StructurePlan &plan, Raised &into) const noexcept override;
};

} // namespace outshine::Generators
#endif
