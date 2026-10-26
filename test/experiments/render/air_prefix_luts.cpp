#include "Atmosphere.h"
#include <array>
#include <bit>
#include <charconv>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <ios>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {
using namespace outshine;

template <typename Value>
bool Write(const std::string &path,
           std::span<const Value> values,
           std::span<const uint32_t> extent = {}) {
  std::ofstream file(path, std::ios::binary);
  if (!extent.empty()) {
    file.write(reinterpret_cast<const char *>(extent.data()),
               static_cast<std::streamsize>(extent.size_bytes()));
  }
  file.write(reinterpret_cast<const char *>(values.data()),
             static_cast<std::streamsize>(values.size_bytes()));
  return static_cast<bool>(file);
}

uint32_t Dimension(std::string_view text) {
  uint32_t value = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  constexpr uint32_t kMaximumDimension = 2048;
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && value > 0 &&
                 value <= kMaximumDimension
             ? value
             : 0;
}
}

int main(int argc, char **argv) {
  static_assert(std::endian::native == std::endian::little);
  if (argc != 2 && argc != 4 && argc != 5) { return 2; }
  const uint32_t width = argc >= 4 ? Dimension(argv[2]) : kTransmittanceLutWidth;
  const uint32_t height = argc >= 4 ? Dimension(argv[3]) : kTransmittanceLutHeight;
  if (width == 0 || height == 0) { return 2; }
  float haze = 0.0f;
  if (argc == 5) {
    const auto text = std::string_view(argv[4]);
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), haze);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
        !std::isfinite(haze) || haze < 0.0f) {
      return 2;
    }
  }
  const Medium medium = Hazed(kEarthAir, haze);
  const std::string folder = argv[1];
  std::vector<float> veil(size_t{width} * height * 4u);
  std::vector<MediumLook> looks(size_t{width} * height);
  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      const auto look = medium_core::mediumTransmittanceTexel(
          medium,
          {.U = (static_cast<float>(x) + 0.5f) / static_cast<float>(width),
           .V = (static_cast<float>(y) + 0.5f) / static_cast<float>(height)},
          {.WidthPx = static_cast<float>(width), .HeightPx = static_cast<float>(height)});
      const auto through = MediumTransmittanceToTop(medium, look, kTransmittanceSteps);
      const size_t pixel = size_t{y} * width + x;
      const size_t offset = pixel * 4u;
      looks[pixel] = look;
      for (size_t channel = 0; channel < 3; ++channel) {
        veil[offset + channel] = through[channel];
      }
      veil[offset + 3] = 1.0f;
    }
  }
  const std::vector<float> multi(size_t{kMultiScatterLutSize} * kMultiScatterLutSize * 4u, 0.0f);
  const std::array<uint32_t, 2> veilExtent = {width, height};
  const std::array<uint32_t, 2> multiExtent = {kMultiScatterLutSize, kMultiScatterLutSize};
  return Write<float>(folder + "/veil-native-f32.bin", veil, veilExtent) &&
                 Write<float>(folder + "/multi.bin", multi, multiExtent) &&
                 Write<MediumLook>(folder + "/looks.bin", looks) &&
                 Write<Medium>(folder + "/medium.bin", std::span<const Medium>(&medium, 1))
             ? 0
             : 2;
}
