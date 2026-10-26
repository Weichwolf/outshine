#ifndef OUTSHINE_RENDER_SCENE_GEOMETRYPAGERESIDENCY_H
#define OUTSHINE_RENDER_SCENE_GEOMETRYPAGERESIDENCY_H

#include <cstdint>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "Vec3.h"
#include "PieceMesh.h"
#include "ResourceHandle.h"
#include "GeometryPageView.h"
#include "TangentFrame.h"

namespace outshine::Render {
class SceneRenderer;
}

namespace outshine::Render {

class GeometryPageResidency {
public:
  void BindRenderer(SceneRenderer *renderer) noexcept { Renderer_ = renderer; }

  void SetFrame(const TangentFrame &frame) { Frame_ = frame; }

  [[nodiscard]] bool Upload(uint32_t tile,
                            const GeometryPageView &baked,
                            const Vec3 &anchorEcef,
                            std::string &error,
                            uint64_t sourceKey = 0);
  [[nodiscard]] bool Upload(uint32_t tile,
                            uint32_t cell,
                            const GeometryPageView &baked,
                            const Vec3 &anchorEcef,
                            std::string &error,
                            uint64_t sourceKey);
  [[nodiscard]] bool StageCell(uint32_t tile,
                               uint32_t cell,
                               const GeometryPageView &baked,
                               const Vec3 &anchorEcef,
                               std::string &error,
                               uint64_t sourceKey);

  struct CellSelection {
    uint32_t Cell = 0;
    std::optional<LevelOfDetail> Detail = LevelOfDetail::Fine;
  };

  struct CellSource {
    uint64_t Key = 0;
    uint64_t Occupied = 0;
  };

  [[nodiscard]] bool ActivateCells(uint32_t tile,
                                   CellSource source,
                                   std::span<const CellSelection> selected,
                                   std::string &error);

  void Retire(uint32_t tile);
  void DiscardStagedCells(uint32_t tile);
  void RetireCell(uint32_t tile, uint32_t cell);
  [[nodiscard]] bool SelectDetail(uint32_t tile, LevelOfDetail detail, std::string &error);
  [[nodiscard]] bool
  SelectDetail(uint32_t tile, uint32_t cell, LevelOfDetail detail, std::string &error);
  [[nodiscard]] bool
  HasCell(uint32_t tile, uint32_t cell, LevelOfDetail detail, uint64_t sourceKey) const noexcept;
  [[nodiscard]] bool HasProjectedTile(uint32_t tile,
                                      CellSource source,
                                      LongitudeLatitude eye,
                                      ProjectedErrorBudget projection,
                                      std::optional<Vec3> eyeEcef = std::nullopt) const noexcept;
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
    for (const auto &stood : Pages_) {
      if (!stood.Visible) { continue; }
      each(DigestRecord{.Tile = stood.Tile,
                        .Cell = stood.Cell,
                        .Digest = stood.Digest,
                        .SourceKey = stood.SourceKey,
                        .FallbackHeights = stood.FallbackHeights,
                        .Detail = stood.Detail});
    }
  }

  [[nodiscard]] std::vector<PieceHandle> Handles() const {
    std::vector<PieceHandle> handles;
    size_t parts = 0;
    for (const auto &page : Pages_) { parts += page.Parts.size(); }
    handles.reserve(parts);
    for (const ResidentGeometryPage &stood : Pages_) {
      handles.insert(handles.end(), stood.Parts.begin(), stood.Parts.end());
    }
    return handles;
  }

  [[nodiscard]] bool ValidateSources(std::string &error) const;

  [[nodiscard]] size_t Uploads() const { return Uploads_; }

  [[nodiscard]] size_t Refused() const { return Refused_; }

  [[nodiscard]] const std::string &WhyRefused() const { return Why_; }

  [[nodiscard]] size_t HeapBytes() const noexcept {
    size_t bytes = Pages_.capacity() * sizeof(ResidentGeometryPage) + Why_.capacity();
    for (const auto &page : Pages_) { bytes += page.Parts.capacity() * sizeof(PieceHandle); }
    return bytes;
  }

private:
  enum class WholeTileTransition : uint8_t { NoCells, Replaced, Failed };

  struct TileCell {
    uint32_t Tile = 0;
    uint32_t Cell = 0;
  };

  struct ResidentGeometryPage {
    uint32_t Tile = 0;
    uint32_t Cell = 0;
    uint64_t Digest = 0;
    uint64_t SourceKey = 0;
    uint64_t OccupiedCells = 0;
    bool FallbackHeights = false;
    std::optional<LevelOfDetail> Detail;
    std::optional<ViewReuseDomain> SelectedView;
    Mat4 Row;
    bool Visible = true;
    std::vector<PieceHandle> Parts;
  };

  [[nodiscard]] static std::pair<uint32_t, uint32_t>
  AddressOf(const ResidentGeometryPage &stood) noexcept {
    return {stood.Tile, stood.Cell};
  }

  [[nodiscard]] Mat4 RowFor(const Vec3 &anchorEcef) const;
  [[nodiscard]] bool ValidateStore(TileCell address,
                                   const GeometryPageView &baked,
                                   uint64_t sourceKey,
                                   bool staged,
                                   std::string &error) const;
  [[nodiscard]] bool ValidateStage(uint32_t tile,
                                   const GeometryPageView &baked,
                                   uint64_t sourceKey,
                                   std::string &error) const;
  [[nodiscard]] bool ValidateActivation(uint32_t tile,
                                        CellSource source,
                                        std::span<const CellSelection> selected,
                                        std::string &error) const;
  [[nodiscard]] bool
  ValidateResidentSources(uint32_t tile, CellSource source, std::string &error) const;
  [[nodiscard]] ResidentGeometryPage *
  CellTarget(uint32_t tile, CellSource source, CellSelection choice) noexcept;
  void DiscardSupersededStage(uint32_t tile, uint64_t sourceKey);
  [[nodiscard]] WholeTileTransition
  RetireCellsForWholeTile(uint32_t tile, uint64_t sourceKey, std::string &error);
  [[nodiscard]] bool Store(uint32_t tile,
                           uint32_t cell,
                           const GeometryPageView &baked,
                           const Vec3 &anchorEcef,
                           std::string &error,
                           uint64_t sourceKey,
                           bool staged);
  [[nodiscard]] bool ShouldShow(uint32_t tile,
                                uint32_t cell,
                                std::optional<LevelOfDetail> detail,
                                uint64_t sourceKey) const;
  void RetireDetail(uint32_t tile, uint32_t cell, std::optional<LevelOfDetail> detail);
  void Releases(const ResidentGeometryPage &stood);
  void RefreshDigest() noexcept;

  SceneRenderer *Renderer_ = nullptr;
  TangentFrame Frame_;
  std::vector<ResidentGeometryPage> Pages_;
  uint64_t Digest_ = 0;
  size_t Uploads_ = 0;
  size_t Refused_ = 0;
  std::string Why_;
};

}
#endif
