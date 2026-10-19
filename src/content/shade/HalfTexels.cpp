#include "HalfTexels.h"
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <vector>

namespace outshine::Core {
namespace {
constexpr size_t kChannels = 4;
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559 && sizeof(uint16_t) == 2);

std::expected<size_t, HalfTexelError> Channels(Texels extent) noexcept {
  constexpr auto pitchMost = static_cast<size_t>(std::numeric_limits<int>::max());
  if (extent.WidthPx == 0 || extent.HeightPx == 0 || extent.HeightPx > pitchMost ||
      extent.WidthPx > pitchMost / kChannels / sizeof(float) ||
      extent.WidthPx >
          std::numeric_limits<size_t>::max() / extent.HeightPx / kChannels / sizeof(float)) {
    return std::unexpected(HalfTexelError::InvalidExtent);
  }
  return size_t{extent.WidthPx} * extent.HeightPx * kChannels;
}
}

std::expected<std::vector<uint8_t>, HalfTexelError> PackHalfTexels(Texels extent,
                                                                   std::span<const float> pixels) {
  const auto channels = Channels(extent);
  if (!channels) { return std::unexpected(channels.error()); }
  if (pixels.size() != *channels) { return std::unexpected(HalfTexelError::InvalidBytes); }
  std::vector<uint8_t> packed(*channels * sizeof(uint16_t));
  const bool converted =
      SDL_ConvertPixels(static_cast<int>(extent.WidthPx),
                        static_cast<int>(extent.HeightPx),
                        SDL_PIXELFORMAT_RGBA128_FLOAT,
                        pixels.data(),
                        static_cast<int>(extent.WidthPx * kChannels * sizeof(float)),
                        SDL_PIXELFORMAT_RGBA64_FLOAT,
                        packed.data(),
                        static_cast<int>(extent.WidthPx * kChannels * sizeof(uint16_t)));
  if (!converted) { return std::unexpected(HalfTexelError::ConversionFailed); }
  return packed;
}

std::expected<std::vector<float>, HalfTexelError>
UnpackHalfTexels(Texels extent, std::span<const uint8_t> pixels) {
  const auto channels = Channels(extent);
  if (!channels) { return std::unexpected(channels.error()); }
  if (pixels.size() < *channels * sizeof(uint16_t)) {
    return std::unexpected(HalfTexelError::InvalidBytes);
  }
  std::vector<float> unpacked(*channels);
  const bool converted =
      SDL_ConvertPixels(static_cast<int>(extent.WidthPx),
                        static_cast<int>(extent.HeightPx),
                        SDL_PIXELFORMAT_RGBA64_FLOAT,
                        pixels.data(),
                        static_cast<int>(extent.WidthPx * kChannels * sizeof(uint16_t)),
                        SDL_PIXELFORMAT_RGBA128_FLOAT,
                        unpacked.data(),
                        static_cast<int>(extent.WidthPx * kChannels * sizeof(float)));
  if (!converted) { return std::unexpected(HalfTexelError::ConversionFailed); }
  return unpacked;
}
}
