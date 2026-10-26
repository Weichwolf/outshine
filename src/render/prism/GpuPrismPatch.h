#ifndef OUTSHINE_RENDER_PRISM_GPUPRISMPATCH_H
#define OUTSHINE_RENDER_PRISM_GPUPRISMPATCH_H

#include "Mat4.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace outshine::Render {
inline constexpr size_t kGpuPrismPatchBytes = 176;

struct alignas(16) GpuPrismPatch {
  Mat4f Current;
  Mat4f Previous;
  std::array<uint32_t, 4> Samples{};
  std::array<float, 4> Shape{};
  std::array<float, 4> Bounds{};

  [[nodiscard]] bool operator==(const GpuPrismPatch &) const = default;
};

static_assert(std::is_standard_layout_v<GpuPrismPatch>);
static_assert(std::is_trivially_copyable_v<GpuPrismPatch>);
static_assert(sizeof(GpuPrismPatch) == kGpuPrismPatchBytes && alignof(GpuPrismPatch) == 16);
static_assert(offsetof(GpuPrismPatch, Current) == 0);
static_assert(offsetof(GpuPrismPatch, Previous) == 64);
static_assert(offsetof(GpuPrismPatch, Samples) == 128);
static_assert(offsetof(GpuPrismPatch, Shape) == 128 + 16);
static_assert(offsetof(GpuPrismPatch, Bounds) == 128 + 2 * 16);

struct alignas(16) PrismDepthView {
  Mat4f ClipFromWorld;
  std::array<float, 4> Shift{};
};

static_assert(std::is_standard_layout_v<PrismDepthView>);
static_assert(std::is_trivially_copyable_v<PrismDepthView>);
static_assert(sizeof(PrismDepthView) == 64 + 16 && alignof(PrismDepthView) == 16);
static_assert(offsetof(PrismDepthView, ClipFromWorld) == 0);
static_assert(offsetof(PrismDepthView, Shift) == 64);
}
#endif
