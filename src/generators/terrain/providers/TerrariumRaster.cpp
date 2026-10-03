#include "TerrariumRaster.h"
#include "GroundSample.h"
#include <webp/decode.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators::Terrain {
namespace {
constexpr uint32_t kMaxRasterSide = 1024;
constexpr size_t kRgbChannels = 3;
constexpr float kHeightOffsetM = 32768.0f;
constexpr float kByteScale = 256.0f;

[[nodiscard]] bool CompleteContainer(std::span<const uint8_t> bytes) {
  constexpr size_t kRiffHeaderBytes = 12;
  if (bytes.size() < kRiffHeaderBytes || std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
      std::memcmp(bytes.data() + 8, "WEBP", 4) != 0) {
    return false;
  }
  const uint32_t payload =
      static_cast<uint32_t>(bytes[4]) | (static_cast<uint32_t>(bytes[5]) << 8u) |
      (static_cast<uint32_t>(bytes[6]) << 16u) | (static_cast<uint32_t>(bytes[7]) << 24u);
  return static_cast<uint64_t>(payload) + 8u == bytes.size();
}
}

std::expected<HeightRaster, std::string> DecodeTerrariumWebp(std::span<const uint8_t> bytes) {
  if (!CompleteContainer(bytes)) {
    return std::unexpected("Terrarium WebP requires one complete RIFF container");
  }
  WebPBitstreamFeatures features{};
  if (WebPGetFeatures(bytes.data(), bytes.size(), &features) != VP8_STATUS_OK ||
      features.format != 2 || features.has_animation != 0 || features.has_alpha != 0 ||
      features.width < 2 || features.height < 2 ||
      std::cmp_greater(features.width, kMaxRasterSide) ||
      std::cmp_greater(features.height, kMaxRasterSide)) {
    return std::unexpected("Terrarium requires a bounded opaque nonanimated lossless WebP raster");
  }
  const auto rows = static_cast<uint32_t>(features.height);
  const auto cols = static_cast<uint32_t>(features.width);
  const size_t stride = static_cast<size_t>(cols) * kRgbChannels;
  std::vector<uint8_t> rgb(static_cast<size_t>(rows) * stride);
  if (WebPDecodeRGBInto(
          bytes.data(), bytes.size(), rgb.data(), rgb.size(), static_cast<int>(stride)) ==
      nullptr) {
    return std::unexpected("Terrarium WebP pixel decoding failed");
  }
  HeightRaster field{
      .Rows = rows, .Cols = cols, .Meters = std::vector<float>(static_cast<size_t>(rows) * cols)};
  for (uint32_t row = 0; row < rows; ++row) {
    for (uint32_t col = 0; col < cols; ++col) {
      const size_t at = static_cast<size_t>(row) * stride + static_cast<size_t>(col) * kRgbChannels;
      const float height = static_cast<float>(rgb[at]) * kByteScale +
                           static_cast<float>(rgb[at + 1]) +
                           static_cast<float>(rgb[at + 2]) / kByteScale - kHeightOffsetM;
      if (!GroundSample::HeightIsOnEarth(height)) {
        return std::unexpected("Terrarium WebP contains a height outside the Earth domain");
      }
      field.Meters[static_cast<size_t>(row) * cols + col] = height;
    }
  }
  return field;
}
}
