#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGSURFACECAPTURE_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGSURFACECAPTURE_H

#include "StructureMesher.h"
#include "BuildingSurface.h"
#include <scene/ProjectedErrorBudget.h>
#include <cstdint>
#include <vector>

namespace outshine::Generators {

struct BuildingScratch;

enum class BuildingSurfaceEmission : uint8_t { FullMesh, VisibleFaces, Patches };

struct BuildingSurfaceCaptureRecord {
  int FaceSizePx = 0;
  std::vector<std::vector<uint8_t>> VisibleFaces;
  std::vector<std::vector<BuildingSurfacePatch>> Patches;
  std::vector<uint8_t> PatchFallbacks;
  std::vector<BuildingSurfaceEmission> Emissions;
};

[[nodiscard]] std::expected<ProjectedStructureMesh, StructureMeshError>
CaptureBuildingSurfaces(std::span<const StructurePlan> plans,
                        const Vec3 &eye,
                        ProjectedErrorBudget projection,
                        BuildingScratch &scratch,
                        BuildingSurfaceCaptureRecord *record = nullptr);

}
#endif
