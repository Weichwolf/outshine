#ifndef OUTSHINE_RENDER_IMPOSTOR_IMPOSTORINSTANCES_H
#define OUTSHINE_RENDER_IMPOSTOR_IMPOSTORINSTANCES_H

#include "ImpostorAtlas.h"
#include "ResourceHandle.h"
#include "math/Mat4.h"
#include "math/Vec3.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace outshine::Render {

class SceneRenderer;

class ImpostorInstances {
public:
  [[nodiscard]] static std::unique_ptr<ImpostorInstances>
  Create(SceneRenderer &renderer,
         const Content::ImpostorAtlas &atlas,
         uint32_t maxInstances,
         std::string &error);
  ~ImpostorInstances();
  ImpostorInstances(const ImpostorInstances &) = delete;
  ImpostorInstances &operator=(const ImpostorInstances &) = delete;

  void MoveTo(SceneRenderer &renderer) noexcept { Renderer_ = &renderer; }

  [[nodiscard]] bool Update(std::span<const Mat4> models, const Vec3 &eye, std::string &error);

private:
  ImpostorInstances(SceneRenderer &renderer, const Vec3 &centre, uint32_t maximum)
      : Renderer_(&renderer), Centre_(centre), MaxInstances_(maximum) {}

  struct View {
    Vec3 Direction;
    PieceHandle Piece{};
    std::vector<Mat4> NextRows;
  };

  SceneRenderer *Renderer_;
  Vec3 Centre_;
  uint32_t MaxInstances_;
  std::vector<View> Views_;
};

}
#endif
