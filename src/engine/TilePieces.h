#ifndef OUTSHINE_ENGINE_TILEPIECES_H
#define OUTSHINE_ENGINE_TILEPIECES_H

#include <cstdint>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <utility>
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
                           std::string &error,
                           uint64_t sourceKey = 0);
  [[nodiscard]] bool Hands(uint32_t tile,
                           uint32_t cell,
                           const Generators::BakedTile &baked,
                           const Vec3 &anchorEcef,
                           std::string &error,
                           uint64_t sourceKey);
  [[nodiscard]] bool StageCell(uint32_t tile,
                               uint32_t cell,
                               const Generators::BakedTile &baked,
                               const Vec3 &anchorEcef,
                               std::string &error,
                               uint64_t sourceKey);

  struct CellSelection {
    uint32_t Cell = 0;
    LevelOfDetail Detail = LevelOfDetail::Fine;
  };

  struct CellSource {
    uint64_t Key = 0;
    uint64_t Occupied = 0;
  };

  [[nodiscard]] bool ActivateCells(uint32_t tile,
                                   CellSource source,
                                   std::span<const CellSelection> selected,
                                   std::string &error);

  void Forgets(uint32_t tile);
  void ForgetsCell(uint32_t tile, uint32_t cell);
  [[nodiscard]] bool SelectDetail(uint32_t tile, LevelOfDetail detail, std::string &error);
  [[nodiscard]] bool
  SelectDetail(uint32_t tile, uint32_t cell, LevelOfDetail detail, std::string &error);
  [[nodiscard]] bool
  HasCell(uint32_t tile, uint32_t cell, LevelOfDetail detail, uint64_t sourceKey) const noexcept;
  [[nodiscard]] bool CellsActive(uint32_t tile,
                                 std::span<const CellSelection> selected,
                                 uint64_t sourceKey) const noexcept;
  void Clear();

  [[nodiscard]] uint64_t Digest() const { return Digest_; }

  struct DigestRecord {
    uint32_t Tile = 0;
    uint32_t Cell = 0;
    uint64_t Digest = 0;
    uint64_t SourceKey = 0;
    bool FallbackHeights = false;
    std::optional<LevelOfDetail> Detail;
  };

  template <typename Each> void ForEachDigest(Each each) const {
    for (const auto &stood : Standing_) {
      if (!stood.Visible) { continue; }
      each(DigestRecord{.Tile = stood.Tile,
                        .Cell = stood.Cell,
                        .Digest = stood.Digest,
                        .SourceKey = stood.SourceKey,
                        .FallbackHeights = stood.FallbackHeights,
                        .Detail = stood.Detail});
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
  enum class WholeTileTransition : uint8_t { NoCells, Replaced, Failed };

  struct TileCell {
    uint32_t Tile = 0;
    uint32_t Cell = 0;
  };

  struct Standing {
    uint32_t Tile = 0;
    uint32_t Cell = 0;
    uint64_t Digest = 0;
    uint64_t SourceKey = 0;
    uint64_t OccupiedCells = 0;
    bool FallbackHeights = false;
    std::optional<LevelOfDetail> Detail;
    Mat4 Row;
    bool Visible = true;
    Render::PieceHandle Walls{};
    Render::PieceHandle Roofs{};
  };

  [[nodiscard]] static std::pair<uint32_t, uint32_t> AddressOf(const Standing &stood) noexcept {
    return {stood.Tile, stood.Cell};
  }

  [[nodiscard]] Mat4 RowFor(const Vec3 &anchorEcef) const;
  [[nodiscard]] bool ValidateStore(TileCell address,
                                   const Generators::BakedTile &baked,
                                   uint64_t sourceKey,
                                   bool staged,
                                   std::string &error) const;
  [[nodiscard]] bool ValidateStage(uint32_t tile,
                                   const Generators::BakedTile &baked,
                                   uint64_t sourceKey,
                                   std::string &error) const;
  [[nodiscard]] bool ValidateActivation(uint32_t tile,
                                        CellSource source,
                                        std::span<const CellSelection> selected,
                                        std::string &error) const;
  [[nodiscard]] bool
  ValidateResidentSources(uint32_t tile, CellSource source, std::string &error) const;
  [[nodiscard]] Standing *
  CellTarget(uint32_t tile, CellSource source, CellSelection choice) noexcept;
  void DiscardSupersededStage(uint32_t tile, uint64_t sourceKey);
  [[nodiscard]] WholeTileTransition
  RetireCellsForWholeTile(uint32_t tile, uint64_t sourceKey, std::string &error);
  [[nodiscard]] bool Store(uint32_t tile,
                           uint32_t cell,
                           const Generators::BakedTile &baked,
                           const Vec3 &anchorEcef,
                           std::string &error,
                           uint64_t sourceKey,
                           bool staged);
  [[nodiscard]] bool ShouldShow(uint32_t tile,
                                uint32_t cell,
                                std::optional<LevelOfDetail> detail,
                                uint64_t sourceKey) const;
  void ForgetsDetail(uint32_t tile, uint32_t cell, std::optional<LevelOfDetail> detail);
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
