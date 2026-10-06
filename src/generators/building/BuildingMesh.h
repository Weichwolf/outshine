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
    return "outshine-building-mesh-11";
  }

  [[nodiscard]] std::optional<double>
  ShellSurfaceErrorM(std::span<const StoredVertex> walls) const noexcept override;

  [[nodiscard]] std::optional<double> ShellSurfaceErrorM(const StructurePlan &plan,
                                                         MeshScratch &lent) const noexcept override;

  [[nodiscard]] std::unique_ptr<MeshScratch> Scratch() const override;

  [[nodiscard]] bool HasSurfaceProjection() const noexcept override { return true; }

  [[nodiscard]] std::expected<ProjectedStructureMesh, StructureMeshError>
  Project(std::span<const StructurePlan> plans,
          const Vec3 &eye,
          ProjectedErrorBudget projection,
          MeshScratch &lent) const override;

  [[nodiscard]] std::optional<Box> SourceEnvelopeBounds(const StructurePlan &plan,
                                                        MeshScratch &lent) const noexcept override;

  [[nodiscard]] std::expected<void, StructureMeshError>
  Mesh(const StructurePlan &plan, MeshScratch &lent, Raised &into) const noexcept override;
};

}
#endif
