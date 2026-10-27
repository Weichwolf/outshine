#ifndef OUTSHINE_RENDER_SCENE_SCENERENDERSTATE_H
#define OUTSHINE_RENDER_SCENE_SCENERENDERSTATE_H

#include "RenderScene.h"
#include "GeometryRenderer.h"
#include <optional>

namespace outshine::Render {
class SceneRenderState {
public:
  SceneRenderState() : Geometry_(Scene_), Transmission_(Scene_, GeometryPassKind::Transmission) {}

  SceneRenderState(const SceneRenderState &) = delete;
  SceneRenderState &operator=(const SceneRenderState &) = delete;
  SceneRenderState(SceneRenderState &&) = delete;
  SceneRenderState &operator=(SceneRenderState &&) = delete;

  [[nodiscard]] RenderScene &Scene() noexcept { return Scene_; }

  [[nodiscard]] const RenderScene &Scene() const noexcept { return Scene_; }

  [[nodiscard]] GeometryRenderer &Geometry() noexcept { return Geometry_; }

  [[nodiscard]] const GeometryRenderer &Geometry() const noexcept { return Geometry_; }

  [[nodiscard]] GeometryRenderer &Transmission() noexcept { return Transmission_; }

  [[nodiscard]] const GeometryRenderer &Transmission() const noexcept { return Transmission_; }

private:
  RenderScene Scene_;
  GeometryRenderer Geometry_;
  GeometryRenderer Transmission_;

public:
  std::optional<GroundClassificationSource> PendingGroundClasses;
  bool DrawsGlass = false;
};
}
#endif
