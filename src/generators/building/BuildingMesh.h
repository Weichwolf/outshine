#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGMESH_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGMESH_H

#include <memory>

#include "FacadeUv.h"
#include "StructureMesher.h"

namespace outshine::Generators {

class BuildingMesh : public StructureMesher {
public:
  [[nodiscard]] std::unique_ptr<MeshScratch> Scratch() const override;

  [[nodiscard]] bool
  Mesh(const StructurePlan &plan, MeshScratch &lent, Raised &into) const noexcept override;
};

} // namespace outshine::Generators
#endif
