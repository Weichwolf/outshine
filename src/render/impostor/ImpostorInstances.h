#ifndef OUTSHINE_RENDER_IMPOSTOR_IMPOSTORINSTANCES_H
#define OUTSHINE_RENDER_IMPOSTOR_IMPOSTORINSTANCES_H

#include "ImpostorAtlas.h"
#include "ImpostorCards.h"
#include "GeometryPasses.h"
#include "ResourceHandle.h"
#include "Mat4.h"
#include "Vec3.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace outshine::Render {

class SceneRenderer;
struct RadianceImage;

class ImpostorInstances {
public:
  [[nodiscard]] static std::unique_ptr<ImpostorInstances> Create(SceneRenderer &renderer,
                                                                 const ImpostorAtlas &atlas,
                                                                 uint32_t maxInstances,
                                                                 std::string &error);
  [[nodiscard]] static std::unique_ptr<ImpostorInstances>
  Create(SceneRenderer &renderer,
         const ImpostorCards &cards,
         uint32_t maxInstances,
         std::string &error,
         std::span<const std::shared_ptr<const RadianceImage>> colours = {},
         GeometryPasses passes = GeometryPasses::MainAndShadow);
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
