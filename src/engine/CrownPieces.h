#ifndef OUTSHINE_ENGINE_CROWNPIECES_H
#define OUTSHINE_ENGINE_CROWNPIECES_H

#include "ImpostorPreparation.h"
#include "ImpostorCard.h"
#include "SubjectTypes.h"
#include "ResourceHandle.h"
#include <memory>

namespace outshine {
namespace Core {
class Live;
}

class CrownPieces {
public:
  static std::unique_ptr<CrownPieces> Create(Core::Live &live,
                                             const Content::ImpostorAtlas &atlas,
                                             uint32_t maxInstances,
                                             std::string &error);
  ~CrownPieces();
  CrownPieces(const CrownPieces &) = delete;
  CrownPieces &operator=(const CrownPieces &) = delete;

  void Into(Core::Live &live) noexcept { Live_ = &live; }

  [[nodiscard]] bool Update(std::span<const Mat4> models, const Vec3 &eye, std::string &error);

private:
  CrownPieces(Core::Live &live, const Vec3 &centre, uint32_t maximum)
      : Live_(&live), Centre_(centre), MaxInstances_(maximum) {}

  struct View {
    Vec3 Direction;
    Render::PieceHandle Piece{};
    std::vector<Mat4> Rows;
    std::vector<Mat4> NextRows;
  };

  Core::Live *Live_;
  Vec3 Centre_;
  uint32_t MaxInstances_;
  std::vector<View> Views_;
};
}
#endif
