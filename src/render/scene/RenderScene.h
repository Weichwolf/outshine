#ifndef OUTSHINE_RENDER_SCENE_RENDERSCENE_H
#define OUTSHINE_RENDER_SCENE_RENDERSCENE_H

#include "GeometryResidency.h"
#include "PrismResidency.h"
#include "terrain/TerrainResidency.h"
#include "SceneResources.h"
#include "TerrainMaterialBuffers.h"

namespace outshine::Render {

class RenderScene {
public:
  RenderScene() = default;

  RenderScene(const RenderScene &) = delete;
  RenderScene &operator=(const RenderScene &) = delete;
  RenderScene(RenderScene &&) = delete;
  RenderScene &operator=(RenderScene &&) = delete;

  ~RenderScene() = default;

  [[nodiscard]] GeometryResidency &Meshes() noexcept { return Meshes_; }

  [[nodiscard]] const GeometryResidency &Meshes() const noexcept { return Meshes_; }

  [[nodiscard]] PrismResidency &Prisms() noexcept { return Prisms_; }

  [[nodiscard]] const PrismResidency &Prisms() const noexcept { return Prisms_; }

  [[nodiscard]] TerrainResidency &Terrain() noexcept { return Terrain_; }

  [[nodiscard]] const TerrainResidency &Terrain() const noexcept { return Terrain_; }

  [[nodiscard]] TerrainMaterialBuffers &GroundMaterials() noexcept { return GroundMaterials_; }

  [[nodiscard]] const TerrainMaterialBuffers &GroundMaterials() const noexcept {
    return GroundMaterials_;
  }

  [[nodiscard]] SceneResources &Resources() noexcept { return Resources_; }

  [[nodiscard]] const SceneResources &Resources() const noexcept { return Resources_; }

private:
  GeometryResidency Meshes_;
  PrismResidency Prisms_;
  TerrainResidency Terrain_;
  TerrainMaterialBuffers GroundMaterials_;
  SceneResources Resources_;
};
}
#endif
