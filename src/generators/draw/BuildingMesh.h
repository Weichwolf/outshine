#ifndef OUTSHINE_GENERATORS_DRAW_BUILDINGMESH_H
#define OUTSHINE_GENERATORS_DRAW_BUILDINGMESH_H

#include "FacadeUv.h"
#include "StructureMesher.h"

namespace outshine::Generators {

class BuildingMesh : public StructureMesher {
public:
  [[nodiscard]] static size_t BuriedTaken();
  [[nodiscard]] static size_t DeepestBuriedMmTaken();
  [[nodiscard]] static size_t RaisedTaken();
  [[nodiscard]] static size_t FarthestMTaken();
  [[nodiscard]] static size_t BoxesTaken();
  [[nodiscard]] static size_t UnscaledTaken();
  [[nodiscard]] static size_t FootlessTaken();
  [[nodiscard]] static size_t PlinthStepsTaken();
  [[nodiscard]] static size_t FloorRimTaken();
  [[nodiscard]] static size_t OverBudgetTaken();

  void Mesh(const StructurePlan &plan, Raised &into) const noexcept override;
};

} // namespace outshine::Generators
#endif
