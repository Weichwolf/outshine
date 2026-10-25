#ifndef OUTSHINE_RENDER_TERRAIN_TERRAINRENDERER_H
#define OUTSHINE_RENDER_TERRAIN_TERRAINRENDERER_H

#include "TerrainLattice.h"

#include <array>
#include <cstdint>
#include <span>
#include <memory>
#include <expected>
#include <string_view>
#include <vector>

#include "Gpu.h"
#include "FrameContext.h"
#include "KernelShape.h"
#include "Vec3.h"
#include "SubjectTypes.h"
#include "TerrainPatchSelection.h"
#include "TerrainResidency.h"
#include <optional>
#include "scene/HeightPageBatch.h"

namespace outshine::Render {
class UploadQueue;

struct SurfaceOutputs;

class TerrainPipelines {
public:
  [[nodiscard]] Native::Result<void> ConfigureLit(Native::Device &device,
                                                  const SurfaceOutputs &outputs,
                                                  std::span<const Native::ColourTarget> targets);
  [[nodiscard]] Native::Result<void> ConfigureDepth(Native::Device &device);

  [[nodiscard]] Native::GraphicsPipeline *Lit() const { return Lit_.get(); }

  [[nodiscard]] Native::GraphicsPipeline *Depth() const { return Depth_.get(); }

  [[nodiscard]] Native::GraphicsPipeline *Patches() const { return Patches_.get(); }

  [[nodiscard]] Native::Result<void> ConfigureSelection(Native::Device &device) {
    return TerrainPatchSelection::Configure(device, Selection_);
  }

  [[nodiscard]] const TerrainPatchSelection::Pipelines &Selection() const noexcept {
    return Selection_;
  }

private:
  std::unique_ptr<Native::GraphicsPipeline> Lit_;
  std::unique_ptr<Native::GraphicsPipeline> Depth_;
  std::unique_ptr<Native::GraphicsPipeline> Patches_;
  TerrainPatchSelection::Pipelines Selection_;
};

class TerrainRenderer {
public:
  explicit TerrainRenderer(TerrainResidency &resident) noexcept
      : Resident_(&resident), Selection_(resident.Patches_) {}

  void UploadsFrom(UploadQueue *queue) noexcept { Resident_->UploadsFrom(queue); }

  static constexpr DrawShape LitShape{.VertexSamplers = 1,
                                      .VertexUniformBuffers = 1,
                                      .FragmentSamplers = 2,
                                      .FragmentUniformBuffers = kSubjectFragmentUniforms,
                                      .FragmentStorageBuffers = 2};
  static constexpr DrawShape PatchShape = [] {
    auto shape = LitShape;
    shape.VertexStorageBuffers = 2;
    shape.FragmentSamplers += 1;
    shape.FragmentUniformBuffers += 1;
    shape.FragmentStorageBuffers += 1;
    return shape;
  }();
  static constexpr DrawShape DepthShape{.VertexSamplers = 1, .VertexUniformBuffers = 1};

  [[nodiscard]] Native::Result<void> Configure(TerrainPipelines &pipelines,
                                               Native::Device &device,
                                               const SurfaceOutputs &outputs,
                                               std::span<const Native::ColourTarget> targets);
  [[nodiscard]] static Native::Result<void> ConfigureDepth(TerrainPipelines &pipelines,
                                                           Native::Device &device);
  [[nodiscard]] Native::Result<void> Configure(Native::Device &device,
                                               const SurfaceOutputs &outputs,
                                               std::span<const Native::ColourTarget> targets);
  [[nodiscard]] Native::Result<void> AttachPipelines(TerrainPipelines &pipelines,
                                                     Native::Device &device);
  [[nodiscard]] Native::Result<void> ReuseInputsFrom(const TerrainRenderer &previous,
                                                     Native::Device &device);

  void UsePipelines(TerrainPipelines &pipelines) noexcept {
    Pipelines_ = &pipelines;
    Selection_.UsePipelines(pipelines.Selection());
  }

  [[nodiscard]] Native::Result<void> SetGrid(std::span<const float> fractions);
  [[nodiscard]] Native::Result<PageId> PlacePage(std::span<const float> nodes);
  [[nodiscard]] Native::Result<std::vector<PageId>>
  PlacePages(std::span<const HeightPageView> nodes);
  void ReleasePage(PageId which);
  [[nodiscard]] Native::Result<void> SetInstances(std::span<const GroundTile> real,
                                                  std::span<const GroundTile> virtual_);
  [[nodiscard]] Native::Result<void>
  Cull(const FrameContext &ctx, const Vec3 &anchorM, Native::Commands &commands);
  [[nodiscard]] Native::Result<void> ConfigurePatchSelection();
  [[nodiscard]] Native::Result<void> SelectPatches(const FrameContext &ctx,
                                                   const Vec3 &anchor,
                                                   const PassRecording &into,
                                                   Native::Buffer &pyramid,
                                                   const PyramidShape &shape);

  [[nodiscard]] Native::Buffer *SelectedPatches() const {
    return Current() ? Selection_.Codes() : nullptr;
  }

  struct DrawArguments {
    std::array<Native::IndexedIndirectArguments, 4> Patches{};
    uint32_t SkirtInstances = 0;
  };

  [[nodiscard]] Native::Result<DrawArguments> ReadDrawArguments() const;

  [[nodiscard]] Native::Result<void> Encode(const PassRecording &into,
                                            std::span<const Native::Binding> bindings) const;
  [[nodiscard]] Native::Result<void> Cast(const PassRecording &into,
                                          const Mat4f &projection,
                                          const Vec4f &shift,
                                          std::span<const Native::Binding> bindings) const;

  [[nodiscard]] uint64_t Generation() const { return Resident_->Generation() + DrawRevision_; }

  [[nodiscard]] uint32_t Instances() const { return Resident_->Instances(); }

  void SetDrawEnabled(bool enabled) {
    if (DrawEnabled_ == enabled) { return; }
    DrawEnabled_ = enabled;
    ++DrawRevision_;
  }

  [[nodiscard]] uint32_t Drawn() const {
    if (!DrawEnabled_) { return 0; }
    return Current() ? VisibleReal_ + VisibleVirtual_ : Resident_->Instances();
  }

  [[nodiscard]] uint32_t PagesStanding() const { return Resident_->PagesStanding(); }

  [[nodiscard]] uint32_t Triangles() const { return Drawn() * (TerrainLattice::kIndices / 3u); }

  [[nodiscard]] uint64_t HeldBytes() const;

private:
  void SyncInputs();

  [[nodiscard]] bool Current() const noexcept {
    return CulledGeneration_ && *CulledGeneration_ == Resident_->Generation();
  }

  [[nodiscard]] Native::Result<void> HandsVisible(Native::Commands &commands);
  [[nodiscard]] Native::Result<void> DrawPatches(const PassRecording &into,
                                                 std::span<const Native::Binding> bindings) const;
  [[nodiscard]] Native::Result<void> Draw(const PassRecording &into,
                                          Native::GraphicsPipeline *pipeline,
                                          const std::unique_ptr<Native::Buffer> &instances,
                                          uint32_t real,
                                          uint32_t virtual_,
                                          std::span<const Native::Binding> bindings,
                                          uint32_t first = 0,
                                          bool skirtsOnly = false) const;

  TerrainResidency *Resident_;
  uint64_t DrawRevision_ = 0;
  std::optional<uint64_t> CulledGeneration_;
  bool DrawEnabled_ = true;
  TerrainPipelines OwnedPipelines_;
  TerrainPipelines *Pipelines_ = &OwnedPipelines_;
  TerrainPatchSelection Selection_;
  std::unique_ptr<Native::Buffer> PatchList_, Visible_, VisibleStaging_;
  std::vector<TerrainInstance> Seen_;
  uint32_t VisibleReal_ = 0, VisibleVirtual_ = 0, VisibleRoom_ = 0;
};
}
#endif
