#ifndef OUTSHINE_RENDER_STAGES_GROUNDPATCHSELECTION_H
#define OUTSHINE_RENDER_STAGES_GROUNDPATCHSELECTION_H

#include "DepthPyramid.h"
#include "FrameContext.h"
#include "Gpu.h"
#include "SubjectTypes.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace outshine::Render {
class GroundPatchSelection {
public:
  GroundPatchSelection();

  struct alignas(16) Bounds {
    std::array<float, 4> Low{};
    std::array<float, 4> High{};
    std::array<float, 4> Errors{};
  };

  static constexpr size_t kBoundsBytes = 48;
  static_assert(sizeof(Bounds) == kBoundsBytes && alignof(Bounds) == 16);
  static_assert(offsetof(Bounds, High) == 16 && offsetof(Bounds, Errors) == 32);

  void Fractions(std::span<const float> fractions);
  void Page(PageId page, std::span<const float> heights);
  void Sources(std::span<const GroundInstance> tiles, uint32_t real);
  void BeginFrame();
  void Include(size_t tile);
  [[nodiscard]] Native::Result<void> Upload(Native::Device &device, Native::Commands &commands);
  [[nodiscard]] Native::Result<void> Configure(Native::Device &device);
  [[nodiscard]] Native::Result<void> Cull(Native::Commands &commands,
                                          const FrameContext &frame,
                                          const Vec3 &anchor,
                                          const Native::Viewport &viewport,
                                          Native::Buffer &pyramid,
                                          const PyramidShape &shape,
                                          const Native::Buffer &candidates,
                                          float errorPx = 0);

  [[nodiscard]] bool Ready() const { return Ready_; }

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

  struct PageRange {
    std::array<std::array<float, 2>, kPatchCount> Patches{};
    std::array<float, 2> Whole{};
    std::array<std::array<float, 4>, kPatchCount> Errors{};
    uint64_t Revision = 0;
    bool Finite = false;
  };

  struct TileRange {
    std::array<Bounds, kPatchCount> Patches{};
    PageId Page = kNoPage;
    uint64_t Revision = 0;
  };

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

  [[nodiscard]] TileRange BoundsFor(const GroundInstance &tile, bool uniform) const;
  [[nodiscard]] Native::Result<void> Reserve(Native::Device &device, uint32_t count);
  [[nodiscard]] Native::Result<void> Select(Native::Commands &commands,
                                            const View &view,
                                            Native::Buffer &pyramid,
                                            const Native::Buffer &candidates);
  std::array<float, kGroundSide> Fractions_{};
  std::vector<PageRange> Pages_;
  std::vector<TileRange> Tiles_;
  std::vector<Bounds> Seen_;
  uint64_t Revision_ = 0;
  bool AlignedFractions_ = true;
  bool UniformFractions_ = true;
  bool FiniteFractions_ = true;
  bool Ready_ = false;
  uint32_t Room_ = 0;
  uint32_t Blocks_ = 0;
  uint64_t ArgumentOffset_ = 0;
  uint64_t CounterOffset_ = 0;
  std::unique_ptr<Native::Buffer> Bounds_, Staging_, Kept_, Slots_, Selected_, Batch_, Arguments_,
      Ranges_, Totals_, Offsets_;
  std::unique_ptr<Native::ComputePipeline> Cull_, LocalScan_, Scan_, Compact_;
};
}
#endif
