#include "TerrariumRaster.h"
#include "Check.h"
#include <webp/encode.h>
#include <array>
#include <memory>
#include <span>
#include <vector>

namespace {
bool Accepts(int width, int height, std::span<const uint8_t> rgb, bool lossless) {
  uint8_t *bytes = nullptr;
  const size_t size = lossless ? WebPEncodeLosslessRGB(rgb.data(), width, height, width * 3, &bytes)
                               : WebPEncodeRGB(rgb.data(), width, height, width * 3, 90.0f, &bytes);
  const std::unique_ptr<uint8_t, decltype(&WebPFree)> owned(bytes, WebPFree);
  CHECK(size > 0 && owned, "independent encoder produces the negative fixture");
  return owned && size > 0 &&
         outshine::Generators::Terrain::DecodeTerrariumWebp({owned.get(), size}).has_value();
}
}

int main() {
  using namespace outshine::Test;
  constexpr std::array<uint8_t, 12> flat = {128, 0, 0, 128, 0, 0, 128, 0, 0, 128, 0, 0};
  CHECK(!Accepts(2, 2, flat, false), "lossy color encoding cannot represent terrain heights");
  CHECK(!Accepts(1, 4, flat, true), "one-column image cannot represent a terrain surface");
  CHECK(!Accepts(4, 1, flat, true), "one-row image cannot represent a terrain surface");
  std::vector<uint8_t> oversized(1025u * 2u * 3u, 128);
  CHECK(!Accepts(1025, 2, oversized, true), "wide raster is refused before pixel allocation");
  CHECK(!Accepts(2, 1025, oversized, true), "tall raster is refused before pixel allocation");
  constexpr std::array<uint8_t, 12> beyondEarth = {128, 0, 0, 128, 0, 0, 128, 0, 0, 255, 255, 255};
  CHECK(!Accepts(2, 2, beyondEarth, true), "one invalid height prevents surface publication");
  CHECK(!outshine::Generators::Terrain::DecodeTerrariumWebp({}), "empty payload is refused");
  return Report();
}
