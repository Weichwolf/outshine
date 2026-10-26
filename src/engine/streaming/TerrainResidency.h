#ifndef OUTSHINE_ENGINE_STREAMING_TERRAINRESIDENCY_H
#define OUTSHINE_ENGINE_STREAMING_TERRAINRESIDENCY_H

#include <Address.h>
#include "FlatMap.h"
#include "device/native/Commands.h"
#include "GroundMesher.h"
#include "TangentFrame.h"
#include "TriangleBvh.h"
#include "scene/TerrainTile.h"
#include "scene/HeightPageBatch.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace outshine {

namespace Render {
class SceneRenderer;
}

enum class TerrainIdentityError { InvalidRequest, Unpublished, NoSource, OverBudget };

enum class TerrainHeapScope { Retained, Additional };

class TerrainResidency {
public:
  TerrainResidency() = default;
  TerrainResidency(const TerrainResidency &other);
  TerrainResidency &operator=(const TerrainResidency &) = delete;
  TerrainResidency(TerrainResidency &&) noexcept = default;
  TerrainResidency &operator=(TerrainResidency &&) noexcept = default;

  void Into(Render::SceneRenderer *renderer) noexcept { Renderer_ = renderer; }

  [[nodiscard]] bool
  Publish(const Patchwork &patchwork, const TangentFrame &frame, std::string &error);
  [[nodiscard]] bool BeginPublish(const Patchwork &patchwork, std::string &error);
  [[nodiscard]] std::expected<bool, std::string>
  AdvancePublish(const Patchwork &patchwork, const TangentFrame &frame, size_t sheetsMost);

  void Clear();

  [[nodiscard]] size_t Standing() const noexcept { return Held_.size(); }

  [[nodiscard]] bool HasPublishedSurface() const noexcept {
    return !Publishing_ && (!Instances_.empty() || !Virtual_.empty());
  }

  [[nodiscard]] size_t Instances() const noexcept { return Instances_.size() + Virtual_.size(); }

  [[nodiscard]] size_t Flat() const noexcept { return Flat_; }

  [[nodiscard]] double LongestBatchMs() const noexcept { return LongestBatchMs_; }

  [[nodiscard]] double FinalizeMs() const noexcept { return FinalizeMs_; }

  [[nodiscard]] uint64_t Digest() const noexcept;
  [[nodiscard]] std::expected<std::array<uint8_t, 32>, TerrainIdentityError>
  SurfaceIdentity(Data::TileId region, size_t pagesMost) const;
  [[nodiscard]] size_t
  HeapBytes(TerrainHeapScope scope = TerrainHeapScope::Retained) const noexcept;
  [[nodiscard]] std::optional<double> HeightMAt(LongitudeLatitude at) const noexcept;
  [[nodiscard]] std::optional<std::array<double, 2>> AslBoundsM() const noexcept;
  [[nodiscard]] std::optional<std::array<double, 2>> SampleAslBoundsM() const noexcept;
  [[nodiscard]] bool
  Occludes(const Ray &ray, float nearM, float distanceM, const TangentFrame &frame) const noexcept;

private:
  struct TileHash {
    [[nodiscard]] uint64_t operator()(const Data::TileId &tile) const noexcept;
  };

  struct Held {
    Data::TileId Tile;
    Render::HeightPageHandle Page{};
    std::shared_ptr<const std::vector<float>> Nodes;
    const std::vector<float> *SharedNodes = nullptr;
    std::array<uint8_t, 32> SamplesDigest{};
    std::array<float, 2> SampleBounds{};
    uint32_t Postings = 0;
    bool Virtual = false;

    [[nodiscard]] std::span<const float> Samples() const noexcept {
      return Nodes ? std::span<const float>(*Nodes) : std::span<const float>{};
    }
  };

  struct PendingPage {
    Held Value;
    size_t Slot = 0;
    bool Appended = false;
  };

  struct PendingSheet {
    Render::TerrainTile Tile;
    size_t Slot = 0;
    uint32_t Postings = 0;
    bool Virtual = false;
  };

  struct PageBatch {
    std::array<PendingPage, Render::kHeightPageBatchMost> Pages;
    std::array<PendingSheet, Render::kHeightPageBatchMost> Sheets;
    size_t PageCount = 0;
    size_t SheetCount = 0;
    size_t Appended = 0;
    size_t Flat = 0;
    size_t End = 0;
  };

  [[nodiscard]] PageBatch
  PlanPages(const Patchwork &patchwork, const TangentFrame &frame, size_t end) const;
  [[nodiscard]] Render::Native::Result<void>
  UploadPages(const Patchwork &patchwork, const TangentFrame &frame, size_t end);
  [[nodiscard]] Render::Native::Result<void>
  CommitPages(PageBatch &batch, std::span<const Render::HeightPageHandle> pages);
  void RetirePages();
  [[nodiscard]] static Render::TerrainTile TileOf(Data::TileId tile,
                                                  Render::HeightPageHandle page,
                                                  std::array<float, 2> samples,
                                                  const TangentFrame &frame);
  [[nodiscard]] static bool HasNativePage(const Sheet &sheet) noexcept;
  [[nodiscard]] Render::Native::Result<void> PublishGrid(const Patchwork &patchwork);
  [[nodiscard]] bool ReindexPages(std::string &error);
  [[nodiscard]] static std::array<uint8_t, 32> SamplesIdentity(std::span<const float> nodes);
  [[nodiscard]] bool AppendSurfacePage(size_t slot, std::vector<size_t> &slots, size_t most) const;
  [[nodiscard]] bool SurfaceLevelPages(size_t zoom,
                                       Data::TileId region,
                                       size_t most,
                                       std::vector<size_t> &slots) const;

  struct PageRange {
    size_t First = 0, End = 0;
  };

  std::vector<Held> Held_;
  std::vector<size_t> Retiring_;
  std::vector<Render::HeightPageHandle> Replaced_;
  FlatMap<size_t, Data::TileId, TileHash> PageIndex_;
  std::array<PageRange, kZoomLevels> PageLevels_{};
  std::vector<Render::TerrainTile> Instances_;
  std::vector<Render::TerrainTile> Virtual_;
  size_t Flat_ = 0;
  uint32_t GridPostings_ = 0;
  Render::SceneRenderer *Renderer_ = nullptr;
  size_t NextSheet_ = 0;
  bool Publishing_ = false;
  double LongestBatchMs_ = 0.0;
  double FinalizeMs_ = 0.0;
};

}
#endif
