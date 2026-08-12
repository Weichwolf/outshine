/* THE BUILDING, as triangles. One outline in, mass and roof and facade out — and every choice in it
 * is a function of the outline, the height the streamer resolved and nothing else. What the two uv
 * floats carry is core/FacadeUv.h's statement and is not repeated here. */
#ifndef BUILDINGMESH_H
#define BUILDINGMESH_H

#include "FacadeUv.h"
#include "StructureMesher.h"

namespace outshine::Generators {

class BuildingMesh : public StructureMesher {
public:
  void Mesh(const StructurePlan &plan, std::vector<float> &soup) const noexcept override;
};

}  // namespace outshine::Generators
#endif
