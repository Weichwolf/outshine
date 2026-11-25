#ifndef OUTSHINE_RENDER_PRISM_PRISMRESIDENCY_H
#define OUTSHINE_RENDER_PRISM_PRISMRESIDENCY_H

#include "GpuPrismPatch.h"
#include "ConvexPrism.h"
#include "GeometryPasses.h"
#include <PrismGrid.h>
#include "PrismInstance.h"
#include "PrismUpload.h"
#include "CasterChanges.h"
#include "Box.h"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace outshine::Render {
class UploadQueue;

struct PrismProduct {
  uint64_t Id = 0;
  uint32_t Material = 0;
  std::shared_ptr<const PrismGrid> Source;
  PrismGridInfo Info;
  std::vector<GpuPrismPatch> Records;
  std::array<std::shared_ptr<Native::Buffer>, 2> Buffers;
  std::optional<GeometryPasses> Passes = GeometryPasses::MainAndShadow;
  std::optional<std::array<float, 4>> WorldSphere = std::nullopt;

  [[nodiscard]] bool Draws(GeometryPasses pass) const noexcept {
    return Passes == GeometryPasses::MainAndShadow || Passes == pass;
  }
};

struct PrismUploadCosts {
  uint64_t SampleBytes = 0;
  uint64_t PatchBytes = 0;
  uint64_t Submissions = 0;
};

class PrismResidency {
public:
  [[nodiscard]] Native::Result<void> Uses(Native::Device &device, UploadQueue *queue);
  [[nodiscard]] bool ReuseFrom(const PrismResidency &source);
  [[nodiscard]] Native::Result<uint64_t>
  Place(std::shared_ptr<const PrismGrid> source,
        std::span<const PrismInstance> instances,
        uint32_t material,
        std::optional<GeometryPasses> passes = GeometryPasses::MainAndShadow);
  [[nodiscard]] Native::Result<uint64_t> PlaceConvex(std::span<const ConvexPrism> shapes,
                                                     uint32_t material,
                                                     std::optional<GeometryPasses> passes);
  void SetPasses(uint64_t id, std::optional<GeometryPasses> passes) noexcept;
  [[nodiscard]] Native::Result<void> SetInstances(uint64_t id,
                                                  std::span<const PrismInstance> instances);
  [[nodiscard]] bool Release(uint64_t id);

  [[nodiscard]] std::span<const PrismProduct> Products() const noexcept { return Products_; }

  [[nodiscard]] const PrismUploadCosts &Uploads() const noexcept { return Costs_; }

  [[nodiscard]] uint64_t Generation() const noexcept { return Generation_; }

  [[nodiscard]] CasterChanges::Snapshot ShadowChangesSince(uint64_t revision) const noexcept {
    return ShadowChanges_.Since(revision);
  }

  void AcknowledgeShadowChanges(uint64_t revision) noexcept {
    ShadowChanges_.Acknowledge(revision);
  }

  [[nodiscard]] Boxf Bounds() const noexcept;

  [[nodiscard]] uint64_t HeldBytes() const noexcept;

private:
  [[nodiscard]] Native::Result<void> ReplaceInstances(PrismProduct &product,
                                                      std::span<const PrismInstance> instances);
  void RecordShadowChanges(std::span<const GpuPrismPatch> previous,
                           std::span<const GpuPrismPatch> records) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 4>>
  ComputeWorldSphere(std::span<const GpuPrismPatch> records) noexcept;

  Native::Device *Device_ = nullptr;
  UploadQueue *Queue_ = nullptr;
  std::vector<PrismProduct> Products_;
  PrismUploadCosts Costs_;
  uint64_t Next_ = 1;
  uint64_t Generation_ = 0;
  CasterChanges ShadowChanges_;
};
}
#endif
