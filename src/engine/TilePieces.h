#ifndef OUTSHINE_ENGINE_TILEPIECES_H
#define OUTSHINE_ENGINE_TILEPIECES_H

#include <cstdint>
#include <cstddef>
#include <optional>
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
    Render::PieceSurface Walls{1};
    Render::PieceSurface Roofs{2};
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
  [[nodiscard]] bool SelectDetail(uint32_t tile, LevelOfDetail detail, std::string &error);
  void Clear();

  [[nodiscard]] uint64_t Digest() const { return Digest_; }

  struct DigestRecord {
    uint32_t Tile = 0;
    uint64_t Digest = 0;
    bool FallbackHeights = false;
  };

  template <typename Each> void ForEachDigest(Each each) const {
    for (const auto &stood : Standing_) {
      if (!stood.Visible) { continue; }
      each(DigestRecord{
          .Tile = stood.Tile, .Digest = stood.Digest, .FallbackHeights = stood.FallbackHeights});
    }
  }

  [[nodiscard]] std::vector<Render::PieceHandle> Handles() const {
    std::vector<Render::PieceHandle> handles;
    handles.reserve(Standing_.size() * 2u);
    for (const Standing &stood : Standing_) {
      if (stood.Walls) { handles.push_back(stood.Walls); }
      if (stood.Roofs) { handles.push_back(stood.Roofs); }
    }
    return handles;
  }

  [[nodiscard]] bool ValidateSources(std::string &error) const;

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
    std::optional<LevelOfDetail> Detail;
    Mat4 Row;
    bool Visible = true;
    Render::PieceHandle Walls{};
    Render::PieceHandle Roofs{};
  };

  [[nodiscard]] Mat4 RowFor(const Vec3 &anchorEcef) const;
  [[nodiscard]] bool ShouldShow(uint32_t tile, std::optional<LevelOfDetail> detail) const;
  void ForgetsDetail(uint32_t tile, std::optional<LevelOfDetail> detail);
  void Releases(const Standing &stood);
  void RefreshDigest() noexcept;

  Render::SceneRenderer *Renderer_ = nullptr;
  TangentFrame Frame_;
  Render::PieceSurface WallsSurface_{1};
  Render::PieceSurface RoofsSurface_{2};
  std::vector<Standing> Standing_;
  uint64_t Digest_ = 0;
  size_t Handed_ = 0;
  size_t Refused_ = 0;
  std::string Why_;
};

}
#endif
