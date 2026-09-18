#ifndef OUTSHINE_ENGINE_CROWNPIECES_H
#define OUTSHINE_ENGINE_CROWNPIECES_H

#include "ImpostorPreparation.h"
#include "ImpostorCard.h"
#include "SubjectTypes.h"
#include "ResourceHandle.h"
#include <memory>

namespace outshine {
namespace Render {
class SceneRenderer;
}

class CrownPieces {
public:
  static std::unique_ptr<CrownPieces> Create(Render::SceneRenderer &renderer,
                                             const Content::ImpostorAtlas &atlas,
                                             uint32_t maxInstances,
                                             std::string &error);
  ~CrownPieces();
  CrownPieces(const CrownPieces &) = delete;
  CrownPieces &operator=(const CrownPieces &) = delete;

  void Into(Render::SceneRenderer &renderer) noexcept { Renderer_ = &renderer; }

  [[nodiscard]] bool Update(std::span<const Mat4> models, const Vec3 &eye, std::string &error);

private:
  CrownPieces(Render::SceneRenderer &renderer, const Vec3 &centre, uint32_t maximum)
      : Renderer_(&renderer), Centre_(centre), MaxInstances_(maximum) {}

  struct View {
    Vec3 Direction;
    Render::PieceHandle Piece{};
    std::vector<Mat4> Rows;
    std::vector<Mat4> NextRows;
  };

  Render::SceneRenderer *Renderer_;
  Vec3 Centre_;
  uint32_t MaxInstances_;
  std::vector<View> Views_;
};
}
#endif
