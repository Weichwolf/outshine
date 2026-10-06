#ifndef OUTSHINE_RENDER_STAGES_GPUPLACEMENT_H
#define OUTSHINE_RENDER_STAGES_GPUPLACEMENT_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace outshine::Render {

inline constexpr size_t kGpuPlacementBytes = 144;

struct alignas(16) GpuPlacement {
  std::array<float, 16> Current{};
  std::array<float, 16> Previous{};
  uint32_t ColourOffset = 0;
  uint32_t TangentOffset = 0;
  std::array<uint32_t, 2> Reserved{};
};

static_assert(std::is_standard_layout_v<GpuPlacement>);
static_assert(std::is_trivially_copyable_v<GpuPlacement>);
static_assert(sizeof(GpuPlacement) == kGpuPlacementBytes);
static_assert(alignof(GpuPlacement) == 16);
static_assert(offsetof(GpuPlacement, Current) == 0);
static_assert(offsetof(GpuPlacement, Previous) == 64);
static_assert(offsetof(GpuPlacement, ColourOffset) == 128);
static_assert(offsetof(GpuPlacement, TangentOffset) == 128 + sizeof(uint32_t));
static_assert(offsetof(GpuPlacement, Reserved) == 128 + 2 * sizeof(uint32_t));

}
#endif
