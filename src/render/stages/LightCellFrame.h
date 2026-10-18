#ifndef OUTSHINE_RENDER_STAGES_LIGHTCELLFRAME_H
#define OUTSHINE_RENDER_STAGES_LIGHTCELLFRAME_H
#include "math/Vec3.h"
#include "math/Vec4.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>

namespace outshine::Render {
inline constexpr size_t kLightCellFrameBytes = 48;

enum class LightCellLookup : int32_t { Hash = 0, Sorted = 1, Full = 2 };

struct alignas(16) LightCellFrame {
  static constexpr uint32_t kLookupWidthM = 32;
  static constexpr double kPositionWidthM = 128;
  static constexpr int32_t kPositionCellLimit = 1 << 30;
  std::array<int32_t, 4> PositionCell{};
  Vec4f PositionLocal;
  std::array<int32_t, 4> LookupCell{};
};

static_assert(sizeof(LightCellFrame) == kLightCellFrameBytes && alignof(LightCellFrame) == 16);
static_assert(offsetof(LightCellFrame, PositionCell) == 0);
static_assert(offsetof(LightCellFrame, PositionLocal) == 16);
static_assert(offsetof(LightCellFrame, LookupCell) == 32);

[[nodiscard]] std::expected<LightCellFrame, std::string> MakeLightCellFrame(const Vec3 &eye,
                                                                            LightCellLookup lookup);

}
#endif
