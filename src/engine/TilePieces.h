#ifndef OUTSHINE_ENGINE_TILEPIECES_H
#define OUTSHINE_ENGINE_TILEPIECES_H

#include <cstdint>
#include <string>
#include <vector>

#include "math/Vec3.h"
#include "SubjectTypes.h"
#include "StructureMesher.h"
#include "TangentFrame.h"

namespace outshine::Core {
class Live;
}

namespace outshine {

class TilePieces {
public:
  void Into(Core::Live *live) { Live_ = live; }

  void Framed(const TangentFrame &frame) { Frame_ = frame; }

  struct Surfaces {
    uint32_t Walls = 1;
    uint32_t Roofs = 2;
  };

  void Wears(Surfaces these) {
    WallsSurface_ = these.Walls;
    RoofsSurface_ = these.Roofs;
  }

  void Hands(uint32_t tile, const Raised &built, const Vec3 &anchorEcef);

  void Forgets(uint32_t tile);
  void Clear();

  [[nodiscard]] uint64_t Digest() const { return Digest_; }

  [[nodiscard]] size_t Handed() const { return Handed_; }

  [[nodiscard]] size_t Refused() const { return Refused_; }

  [[nodiscard]] const std::string &WhyRefused() const { return Why_; }

private:
  struct Standing {
    uint32_t Tile = 0;
    Render::PieceId Walls = Render::kNoPiece;
    Render::PieceId Roofs = Render::kNoPiece;
  };

  [[nodiscard]] Mat4 RowFor(const Vec3 &anchorEcef) const;

  Core::Live *Live_ = nullptr;
  TangentFrame Frame_;
  uint32_t WallsSurface_ = 1;
  uint32_t RoofsSurface_ = 2;
  std::vector<Standing> Standing_;
  uint64_t Digest_ = 0;
  size_t Handed_ = 0;
  size_t Refused_ = 0;
  std::string Why_;
};

} // namespace outshine
#endif
