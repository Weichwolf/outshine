#ifndef OUTSHINE_RENDER_TERRAIN_TERRAINRESIDENCY_H
#define OUTSHINE_RENDER_TERRAIN_TERRAINRESIDENCY_H

#include "GroundTile.h"
#include "TerrainPatchBounds.h"
#include "scene/HeightPageBatch.h"
#include "device/native/Device.h"
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace outshine::Render {
class UploadQueue;
class TerrainRenderer;

class TerrainResidency {
public:
  void UploadsFrom(UploadQueue *queue) noexcept { Uploads_ = queue; }

  [[nodiscard]] Native::Result<void> StandsOn(Native::Device &device);
  [[nodiscard]] Native::Result<void> ReuseInputsFrom(const TerrainResidency &previous,
                                                     Native::Device &device);
  [[nodiscard]] Native::Result<void> SetGrid(std::span<const float> fractions);
  [[nodiscard]] Native::Result<PageId> PlacePage(std::span<const float> nodes);
  [[nodiscard]] Native::Result<std::vector<PageId>>
  PlacePages(std::span<const HeightPageView> nodes);
  void ReleasePage(PageId which);
  [[nodiscard]] Native::Result<void> SetInstances(std::span<const GroundTile> real,
                                                  std::span<const GroundTile> virtual_);

  [[nodiscard]] uint64_t Generation() const noexcept { return Generation_; }

  [[nodiscard]] uint32_t Instances() const noexcept { return RealCount_ + VirtualCount_; }

  [[nodiscard]] uint32_t PagesStanding() const noexcept { return PagesLive_; }

  [[nodiscard]] uint64_t HeldBytes() const;

private:
  friend class TerrainRenderer;
  [[nodiscard]] Native::Result<void> BuildGrid(std::span<const float> fractions,
                                               std::shared_ptr<const Native::Buffer> &into);
  [[nodiscard]] Native::Result<void> BuildPages(uint32_t layers,
                                                std::unique_ptr<Native::Texture> &into);
  [[nodiscard]] Native::Result<void> BuildPatchGrid();
  [[nodiscard]] Native::Result<std::unique_ptr<Native::Buffer>>
  PatchFractions(std::span<const float> fractions);
  [[nodiscard]] Native::Result<std::unique_ptr<Native::Buffer>>
  PatchBuffer(std::span<const std::byte> bytes, Native::BufferUse use);

  uint64_t Generation_ = 0;
  UploadQueue *Uploads_ = nullptr;
  Native::Device *Device_ = nullptr;
  TerrainPatchBounds Patches_;
  std::unique_ptr<Native::Texture> Pages_;
  std::unique_ptr<Native::Sampler> Nearest_;
  std::shared_ptr<const Native::Buffer> Grid_, UniformGrid_, Index_, PatchGrid_, PatchIndex_,
      PatchFractions_;
  std::unique_ptr<Native::Buffer> Instances_;
  std::vector<TerrainInstance> Held_;
  std::vector<std::array<float, 4>> Bounds_;
  std::vector<PageId> Spare_;
  std::vector<uint8_t> PagesInUse_;
  uint32_t PagesMade_ = 0, PageLayers_ = 0, PagesLive_ = 0, InstanceRoom_ = 0;
  uint32_t RealCount_ = 0, VirtualCount_ = 0;
};
}
#endif
