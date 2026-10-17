#ifndef OUTSHINE_RENDER_STAGES_GROUNDLATTICE_H
#define OUTSHINE_RENDER_STAGES_GROUNDLATTICE_H

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
#include "math/Vec3.h"
#include "SubjectTypes.h"
#include "GroundPatchSelection.h"

namespace outshine::Render {
class UploadQueue;

struct SurfaceOutputs;

class GroundPipelineBinding {
public:
  [[nodiscard]] Native::Result<void> ConfigureLit(Native::Device &device,
                                                  const SurfaceOutputs &outputs,
                                                  std::span<const Native::ColourTarget> targets);
  [[nodiscard]] Native::Result<void> ConfigureDepth(Native::Device &device);

  [[nodiscard]] Native::GraphicsPipeline *Lit() const { return Lit_.get(); }

  [[nodiscard]] Native::GraphicsPipeline *Depth() const { return Depth_.get(); }

  [[nodiscard]] Native::GraphicsPipeline *Patches() const { return Patches_.get(); }

  [[nodiscard]] Native::Result<void> ConfigureSelection(Native::Device &device) {
    return GroundPatchSelection::Configure(device, Selection_);
  }

  [[nodiscard]] const GroundPatchSelection::Pipelines &Selection() const noexcept {
    return Selection_;
  }

private:
  std::unique_ptr<Native::GraphicsPipeline> Lit_;
  std::unique_ptr<Native::GraphicsPipeline> Depth_;
  std::unique_ptr<Native::GraphicsPipeline> Patches_;
  GroundPatchSelection::Pipelines Selection_;
};

class GroundLattice {
public:
  void UploadsFrom(UploadQueue *queue) noexcept { Uploads_ = queue; }

#define GROUND_INT(name, value) static constexpr int name = value;
#define GROUND_UINT(name, value) static constexpr uint32_t name = value;
#define GROUND_FLOAT(name, value) static constexpr float name = value;
#include "GroundConstants.inc"
#undef GROUND_INT
#undef GROUND_UINT
#undef GROUND_FLOAT
  static constexpr int kSide = kGroundSide;
  static constexpr int kPageSide = kSide + 2;
  static constexpr uint32_t kPageNodes =
      static_cast<uint32_t>(kPageSide) * static_cast<uint32_t>(kPageSide);
  static constexpr float kSkirtSteps = kGroundSkirtSteps;
  static constexpr uint32_t kNodes = static_cast<uint32_t>(kSide) * static_cast<uint32_t>(kSide);
  static constexpr uint32_t kPageColumns = kGroundPageColumns;
  static constexpr uint32_t kPagesPerLayer = kPageColumns * kPageColumns;
  static constexpr uint32_t kMaximumPageLayers = 256;
  static constexpr uint32_t kMaximumPages = kMaximumPageLayers * kPagesPerLayer;
  static constexpr uint32_t kGridFloats = 3;
  static constexpr uint32_t kVertices = kNodes + 4u * static_cast<uint32_t>(kSide);
  static constexpr uint32_t kQuads =
      static_cast<uint32_t>(kSide - 1) * static_cast<uint32_t>(kSide - 1);
  static constexpr uint32_t kSkirtQuads = 4u * static_cast<uint32_t>(kSide - 1);
  static constexpr uint32_t kIndices = (kQuads + kSkirtQuads) * 6u;
  static constexpr uint32_t kPatchSide = kGroundPatchCells + 1u;
  static constexpr uint32_t kPatchCount = 1u << kGroundPatchBits;
  static constexpr uint32_t kPatchIndices = kGroundPatchCells * kGroundPatchCells * 6u;
  static constexpr DrawShape LitShape{.VertexSamplers = 1,
                                      .VertexUniformBuffers = 1,
                                      .FragmentSamplers = 2,
                                      .FragmentUniformBuffers = kSubjectFragmentUniforms,
                                      .FragmentStorageBuffers = 2};
  static constexpr DrawShape PatchShape = [] {
    auto shape = LitShape;
    shape.VertexStorageBuffers = 2;
    shape.FragmentSamplers += 1;
    shape.FragmentStorageBuffers += 1;
    return shape;
  }();
  static constexpr DrawShape DepthShape{.VertexSamplers = 1, .VertexUniformBuffers = 1};

  [[nodiscard]] Native::Result<void> Configure(GroundPipelineBinding &pipelines,
                                               Native::Device &device,
                                               const SurfaceOutputs &outputs,
                                               std::span<const Native::ColourTarget> targets);
  [[nodiscard]] static Native::Result<void> ConfigureDepth(GroundPipelineBinding &pipelines,
                                                           Native::Device &device);
  [[nodiscard]] Native::Result<void> Configure(Native::Device &device,
                                               const SurfaceOutputs &outputs,
                                               std::span<const Native::ColourTarget> targets);
  [[nodiscard]] Native::Result<void> AttachPipelines(GroundPipelineBinding &pipelines,
                                                     Native::Device &device);

  void UsePipelines(GroundPipelineBinding &pipelines) noexcept {
    Pipelines_ = &pipelines;
    Selection_.UsePipelines(pipelines.Selection());
  }

  [[nodiscard]] Native::Result<void> SetGrid(std::span<const float> fractions);
  [[nodiscard]] Native::Result<PageId> PlacePage(std::span<const float> nodes);
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

  [[nodiscard]] Native::Buffer *SelectedPatches() const { return Selection_.Codes(); }

  [[nodiscard]] Native::Result<void> Encode(const PassRecording &into,
                                            std::span<const Native::Binding> bindings) const;
  [[nodiscard]] Native::Result<void> Cast(const PassRecording &into,
                                          const Mat4f &projection,
                                          const Vec4f &shift,
                                          std::span<const Native::Binding> bindings) const;

  [[nodiscard]] uint64_t Generation() const { return Generation_; }

  [[nodiscard]] uint32_t Instances() const { return RealCount_ + VirtualCount_; }

  void SetDrawEnabled(bool enabled) {
    if (DrawEnabled_ == enabled) { return; }
    DrawEnabled_ = enabled;
    ++Generation_;
  }

  [[nodiscard]] uint32_t Drawn() const {
    return DrawEnabled_ ? VisibleReal_ + VisibleVirtual_ : 0u;
  }

  [[nodiscard]] uint32_t PagesStanding() const { return PagesLive_; }

  [[nodiscard]] uint32_t Triangles() const { return Drawn() * (kIndices / 3u); }

  [[nodiscard]] uint64_t HeldBytes() const;

private:
  [[nodiscard]] Native::Result<void> BuildGrid(std::span<const float> fractions,
                                               std::unique_ptr<Native::Buffer> &into);
  [[nodiscard]] Native::Result<void> BuildPages(uint32_t layers,
                                                std::unique_ptr<Native::Texture> &into);
  [[nodiscard]] Native::Result<void> ReservePages(uint32_t count);
  [[nodiscard]] Native::Result<void> HandsVisible(Native::Commands &commands);
  [[nodiscard]] Native::Result<void> BuildPatchGrid();
  [[nodiscard]] Native::Result<std::unique_ptr<Native::Buffer>>
  PatchFractions(std::span<const float> fractions);
  [[nodiscard]] Native::Result<std::unique_ptr<Native::Buffer>>
  PatchBuffer(std::span<const std::byte> bytes, Native::BufferUse use);
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

  uint64_t Generation_ = 0;
  bool DrawEnabled_ = true;
  UploadQueue *Uploads_ = nullptr;
  Native::Device *Device_ = nullptr;
  GroundPipelineBinding OwnedPipelines_;
  GroundPipelineBinding *Pipelines_ = &OwnedPipelines_;
  GroundPatchSelection Selection_;
  std::unique_ptr<Native::Texture> Pages_;
  std::unique_ptr<Native::Sampler> Nearest_;
  std::unique_ptr<Native::Buffer> Grid_;
  std::unique_ptr<Native::Buffer> UniformGrid_;
  std::unique_ptr<Native::Buffer> Index_;
  std::unique_ptr<Native::Buffer> PatchGrid_;
  std::unique_ptr<Native::Buffer> PatchIndex_;
  std::unique_ptr<Native::Buffer> PatchFractions_;
  std::unique_ptr<Native::Buffer> PatchList_;
  std::unique_ptr<Native::Buffer> Instances_;
  std::unique_ptr<Native::Buffer> Visible_;
  std::unique_ptr<Native::Buffer> VisibleStaging_;
  std::vector<GroundInstance> Held_;
  std::vector<std::array<float, 4>> Bounds_;
  std::vector<GroundInstance> Seen_;
  std::vector<PageId> Spare_;
  std::vector<uint8_t> PagesInUse_;
  uint32_t PagesMade_ = 0;
  uint32_t PageLayers_ = 0;
  uint32_t PagesLive_ = 0;
  uint32_t InstanceRoom_ = 0;
  uint32_t RealCount_ = 0;
  uint32_t VirtualCount_ = 0;
  uint32_t VisibleReal_ = 0;
  uint32_t VisibleVirtual_ = 0;
  uint32_t VisibleRoom_ = 0;
};

}
#endif
