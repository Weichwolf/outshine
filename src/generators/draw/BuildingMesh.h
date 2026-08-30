#ifndef OUTSHINE_GENERATORS_DRAW_BUILDINGMESH_H
#define OUTSHINE_GENERATORS_DRAW_BUILDINGMESH_H

#include "FacadeUv.h"
#include "StructureMesher.h"

namespace outshine::Generators {

class BuildingMesh : public StructureMesher {
public:
  // BUILDINGS WHOSE SEAT LIES BELOW THE GROUND THEY STAND ON, since the last read. A building is
  // placed on the HIGHEST ground its footprint touches, so this can only be non-zero if the ground
  // rises somewhere the placement did not sample -- which is the measure's own limit and the reason
  // it exists rather than an assertion.
  [[nodiscard]] static size_t BuriedTaken();
  // AND THE DEEPEST ANY OF THEM IS BURIED, in millimetres, since the last read.
  [[nodiscard]] static size_t DeepestBuriedMmTaken();
  [[nodiscard]] static size_t RaisedTaken();
  [[nodiscard]] static size_t FarthestMTaken();
  [[nodiscard]] static size_t BoxesTaken();
  [[nodiscard]] static size_t UnscaledTaken();
  [[nodiscard]] static size_t FootlessTaken();
  [[nodiscard]] static size_t PlinthStepsTaken();
  [[nodiscard]] static size_t FloorRimTaken();
  [[nodiscard]] static size_t OverBudgetTaken();

  void Mesh(const StructurePlan &plan, std::vector<float> &soup) const noexcept override;
};

} // namespace outshine::Generators
#endif
