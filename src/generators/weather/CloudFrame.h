#ifndef OUTSHINE_GENERATORS_WEATHER_CLOUDFRAME_H
#define OUTSHINE_GENERATORS_WEATHER_CLOUDFRAME_H

#include "CloudLayer.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <type_traits>

namespace outshine::Generators::Weather {

struct alignas(16) CloudFrame {
  std::array<float, 4> Shell{};
  std::array<float, 4> Field{};
  std::array<float, 4> CoordinateOrigin{};
  std::array<uint32_t, 4> BroadBase{};
  std::array<float, 4> BroadFraction{};
  std::array<uint32_t, 4> DetailBase{};
  std::array<float, 4> DetailFraction{};
};

static_assert(sizeof(float) == 4 && sizeof(uint32_t) == 4);
static_assert(std::is_standard_layout_v<CloudFrame> && std::is_trivially_copyable_v<CloudFrame>);
static_assert(sizeof(CloudFrame) == 112 && alignof(CloudFrame) == 16);
static_assert(offsetof(CloudFrame, Shell) == 0);
static_assert(offsetof(CloudFrame, Field) == 16);
static_assert(offsetof(CloudFrame, CoordinateOrigin) == 32);
static_assert(offsetof(CloudFrame, BroadBase) == 48);
static_assert(offsetof(CloudFrame, BroadFraction) == 64);
static_assert(offsetof(CloudFrame, DetailBase) == 80);
static_assert(offsetof(CloudFrame, DetailFraction) == 96);

enum class CloudFrameFailure { InvalidLayer, InvalidPhase, InvalidOrigin, Unrepresentable };

[[nodiscard]] std::expected<CloudFrame, CloudFrameFailure>
MakeCloudFrame(const CloudLayer &layer, double phaseS, const Vec3 &coordinateOriginM = {}) noexcept;

}
#endif
