#ifndef OUTSHINE_RENDER_TERRAIN_TERRAINPATCHSELECTION_H
#define OUTSHINE_RENDER_TERRAIN_TERRAINPATCHSELECTION_H

#include "DepthPyramid.h"
#include "TerrainPatchBounds.h"
#include "FrameContext.h"
#include "Gpu.h"
#include "GroundTile.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace outshine::Render {
class TerrainPatchSelection {
public:
  explicit TerrainPatchSelection(const TerrainPatchBounds &bounds) noexcept : Source_(&bounds) {}

  struct Pipelines {
    std::unique_ptr<Native::ComputePipeline> Cull, LocalScan, Scan, Compact;
  };

  void UsePipelines(const Pipelines &pipelines) noexcept { Pipelines_ = &pipelines; }

  using Bounds = TerrainPatchBounds::Bounds;

  void BeginFrame();
  void Include(size_t tile);
  [[nodiscard]] Native::Result<void> Upload(Native::Device &device, Native::Commands &commands);
  [[nodiscard]] static Native::Result<void> Configure(Native::Device &device, Pipelines &pipelines);
  [[nodiscard]] Native::Result<void> Cull(Native::Commands &commands,
                                          const FrameContext &frame,
                                          const Vec3 &anchor,
                                          const Native::Viewport &viewport,
                                          Native::Buffer &pyramid,
                                          const PyramidShape &shape,
                                          const Native::Buffer &candidates,
                                          float errorPx = 0);

  [[nodiscard]] bool Ready() const { return Ready_ && SourceRevision_ == Source_->Generation(); }

  [[nodiscard]] Native::Buffer *Codes() const { return Selected_.get(); }

  [[nodiscard]] const Native::Buffer *Arguments() const { return Arguments_.get(); }

  [[nodiscard]] uint64_t ArgumentOffset() const { return ArgumentOffset_; }

  [[nodiscard]] uint64_t HeldBytes() const;

private:
#define GROUND_INT(name, value) static constexpr int name = value;
#define GROUND_UINT(name, value) static constexpr uint32_t name = value;
#define GROUND_FLOAT(name, value) static constexpr float name = value;
#include "GroundConstants.inc"
#undef GROUND_INT
#undef GROUND_UINT
#undef GROUND_FLOAT
  static constexpr size_t kPatchCount = 1u << kGroundPatchBits;

  struct alignas(16) View {
    Mat4f Clip{};
    Vec4f Shift{};
    std::array<float, 4> Viewport{};
    std::array<uint32_t, 4> Wide{}, High{}, At{};
    std::array<uint32_t, 4> Source{};
    std::array<uint32_t, 4> Work{};
    std::array<float, 4> Detail{};
  };

  static constexpr size_t kViewBytes = 192;
  static constexpr size_t kViewPyramidOffset = 96;
  static constexpr size_t kViewSourceOffset = 144;
  static_assert(sizeof(View) == kViewBytes && offsetof(View, Wide) == kViewPyramidOffset);
  static_assert(offsetof(View, Source) == kViewSourceOffset);
  static_assert(offsetof(View, Work) == kViewSourceOffset + 16);
  static_assert(offsetof(View, Detail) == kViewBytes - 16);

  [[nodiscard]] Native::Result<void> Reserve(Native::Device &device, uint32_t count);
  [[nodiscard]] Native::Result<void> Select(Native::Commands &commands,
                                            const View &view,
                                            Native::Buffer &pyramid,
                                            const Native::Buffer &candidates);
  const TerrainPatchBounds *Source_;
  std::vector<Bounds> Seen_;
  uint64_t SourceRevision_ = 0;
  bool Ready_ = false;
  uint32_t Room_ = 0;
  uint32_t Blocks_ = 0;
  uint64_t ArgumentOffset_ = 0;
  uint64_t CounterOffset_ = 0;
  std::unique_ptr<Native::Buffer> Bounds_, Staging_, Kept_, Slots_, Selected_, Batch_, Arguments_,
      Ranges_, Totals_, Offsets_;
  const Pipelines *Pipelines_ = nullptr;
};
}
#endif
