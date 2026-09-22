#ifndef OUTSHINE_ENGINE_TILEPIECES_H
#define OUTSHINE_ENGINE_TILEPIECES_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

#include "math/Vec3.h"
#include "SubjectTypes.h"
#include "ResourceHandle.h"
#include "StructureBake.h"
#include "TangentFrame.h"

namespace outshine::Render {
class SceneRenderer;
}

namespace outshine {

class TilePieces {
public:
  void Into(Render::SceneRenderer *renderer) noexcept { Renderer_ = renderer; }

  void Framed(const TangentFrame &frame) { Frame_ = frame; }

  struct Surfaces {
    uint32_t Walls = 1;
    uint32_t Roofs = 2;
  };

  void Wears(Surfaces these) noexcept {
    WallsSurface_ = these.Walls;
    RoofsSurface_ = these.Roofs;
  }

  [[nodiscard]] bool Hands(uint32_t tile,
                           const Generators::BakedTile &baked,
                           const Vec3 &anchorEcef,
                           std::string &error);

  void Forgets(uint32_t tile);
  void Clear();

  [[nodiscard]] uint64_t Digest() const { return Digest_; }

  struct DigestRecord {
    uint32_t Tile = 0;
    uint64_t Digest = 0;
    bool FallbackHeights = false;
  };

  template <typename Each> void ForEachDigest(Each each) const {
    for (const auto &stood : Standing_) {
      each(DigestRecord{
          .Tile = stood.Tile, .Digest = stood.Digest, .FallbackHeights = stood.FallbackHeights});
    }
  }

  [[nodiscard]] size_t Handed() const { return Handed_; }

  [[nodiscard]] size_t Refused() const { return Refused_; }

  [[nodiscard]] const std::string &WhyRefused() const { return Why_; }

  [[nodiscard]] size_t HeapBytes() const noexcept {
    return Standing_.capacity() * sizeof(Standing) + Why_.capacity();
  }

private:
  struct Standing {
    uint32_t Tile = 0;
    uint64_t Digest = 0;
    bool FallbackHeights = false;
    Render::PieceHandle Walls{};
    Render::PieceHandle Roofs{};
  };

  [[nodiscard]] Mat4 RowFor(const Vec3 &anchorEcef) const;
  void RefreshDigest() noexcept;

  Render::SceneRenderer *Renderer_ = nullptr;
  TangentFrame Frame_;
  uint32_t WallsSurface_ = 1;
  uint32_t RoofsSurface_ = 2;
  std::vector<Standing> Standing_;
  uint64_t Digest_ = 0;
  size_t Handed_ = 0;
  size_t Refused_ = 0;
  std::string Why_;
};

}
#endif
