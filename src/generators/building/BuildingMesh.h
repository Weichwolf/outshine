#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGMESH_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGMESH_H

#include <memory>
#include <optional>
#include <span>

#include "FacadeUv.h"
#include "StructureMesher.h"

namespace outshine::Generators {

class BuildingMesh : public StructureMesher {
public:
  [[nodiscard]] std::string_view ArtifactVersion() const noexcept override {
    return "outshine-building-mesh-9";
  }

  [[nodiscard]] std::optional<double>
  ShellSurfaceErrorM(std::span<const StoredVertex> walls) const noexcept override;

  [[nodiscard]] std::optional<double> ShellSurfaceErrorM(const StructurePlan &plan,
                                                         MeshScratch &lent) const noexcept override;

  [[nodiscard]] std::unique_ptr<MeshScratch> Scratch() const override;

  [[nodiscard]] std::expected<void, StructureMeshError>
  Mesh(const StructurePlan &plan, MeshScratch &lent, Raised &into) const noexcept override;
};

}
#endif
