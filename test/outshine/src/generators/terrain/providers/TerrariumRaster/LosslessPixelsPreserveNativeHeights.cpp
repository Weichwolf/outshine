#include "TerrariumRaster.h"
#include "Check.h"
#include <webp/encode.h>
#include <array>
#include <memory>
#include <vector>

int main() {
  using namespace outshine::Test;
  constexpr std::array<float, 4> heights = {-1.5f, 0.0f, 172.875f, 8848.125f};
  constexpr std::array<uint8_t, 12> rgb = {127, 254, 128, 128, 0, 0, 128, 172, 224, 162, 144, 32};
  uint8_t *encoded = nullptr;
  const size_t count = WebPEncodeLosslessRGB(rgb.data(), 2, 2, 6, &encoded);
  const std::unique_ptr<uint8_t, decltype(&WebPFree)> owned(encoded, WebPFree);
  CHECK(count > 0 && owned, "independent libwebp encoder creates lossless RGB fixture");
  if (count == 0 || !owned) { return Report(); }
  auto field = outshine::Generators::Terrain::DecodeTerrariumWebp({owned.get(), count});
  CHECK(field.has_value(), "native terrain decoder accepts valid lossless fixture");
  if (field) {
    CHECK(field->Rows == 2 && field->Cols == 2, "native dimensions preserved");
    for (uint32_t row = 0; row < 2; ++row) {
      for (uint32_t col = 0; col < 2; ++col) {
        CHECK_NEAR(field->Meters[row * 2 + col],
                   heights[row * 2 + col],
                   0.0f,
                   "m",
                   "RGB fractional height is preserved exactly, including below sea level");
      }
    }
  }
  std::vector<uint8_t> truncated(owned.get(), owned.get() + count - 1);
  CHECK(!outshine::Generators::Terrain::DecodeTerrariumWebp(truncated),
        "truncated successful HTTP payload cannot become a native raster");
  std::vector<uint8_t> trailing(owned.get(), owned.get() + count);
  trailing.push_back(0);
  CHECK(!outshine::Generators::Terrain::DecodeTerrariumWebp(trailing),
        "bytes outside declared container cannot be silently accepted");
  return Report();
}
