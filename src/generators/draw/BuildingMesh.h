#ifndef OUTSHINE_WORLD_GENERATORS_DRAW_BUILDINGMESH_H
#define OUTSHINE_WORLD_GENERATORS_DRAW_BUILDINGMESH_H

#include "FacadeUv.h"
#include "StructureMesher.h"

namespace outshine::Generators {

class BuildingMesh : public StructureMesher {
public:
  void Mesh(const StructurePlan &plan, std::vector<float> &soup) const noexcept override;
};

}
#endif
