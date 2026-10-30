#include "Check.h"
#include "ImageMips.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

std::vector<uint8_t> Bytes(std::span<const float> values) {
  std::vector<uint8_t> bytes(values.size_bytes());
  std::memcpy(bytes.data(), values.data(), bytes.size());
  return bytes;
}

void Samples() {
  const std::array<float, 16> values{1.25f,
                                     .125f,
                                     65537,
                                     0,
                                     2.25f,
                                     .375f,
                                     65537,
                                     .25f,
                                     3.25f,
                                     .625f,
                                     65537,
                                     .5f,
                                     4.25f,
                                     .875f,
                                     65537,
                                     1};
  const auto base = Bytes(values);
  const ImageView image{
      .WidthPx = 2, .HeightPx = 2, .Format = ImagePixelFormat::Rgba32Float, .Rgba = base};
  CHECK(image.valid() && ImagePixelBytes(image.Format) == 16 &&
            LowerImageMipBytes(2, 2, image.Format) == 16,
        "Float32 image sizes preserve all four components");
  const std::array<float, 4> mean{2.75f, .5f, 65537, .4375f};
  const auto expected = Bytes(mean);
  const auto linear = Core::PrepareImageMips(image, ImageMipKind::Linear);
  const auto colour = Core::PrepareImageMips(image, ImageMipKind::Colour);
  CHECK(linear && *linear == expected && colour == linear,
        "floating colours are already linear and retain values beyond byte and half range");
  const std::array<float, 8> opposed{1, .5f, .5f, .125f, 0, .5f, .5f, .75f};
  const auto normalBytes = Bytes(opposed);
  const auto normal = Core::PrepareImageMips(
      {.WidthPx = 2, .HeightPx = 1, .Format = image.Format, .Rgba = normalBytes},
      ImageMipKind::Normal);
  const std::array<float, 4> cancelled{.5f, .5f, 1, 0};
  CHECK(normal && *normal == Bytes(cancelled),
        "cancelled normal moments retain the stable forward fallback and zero weight");
  auto invalidValues = values;
  invalidValues[1] = std::numeric_limits<float>::quiet_NaN();
  const auto invalidBytes = Bytes(invalidValues);
  auto invalid = image;
  invalid.Rgba = invalidBytes;
  CHECK(!Core::PrepareImageMips(invalid, ImageMipKind::Linear),
        "nonfinite floating samples cannot create a valid mip chain");
  invalid = image;
  invalid.Rgba = std::span(base).first(base.size() - 1);
  CHECK(!invalid.valid() && !Core::PrepareImageMips(invalid, ImageMipKind::Linear),
        "truncated Float32 pixel storage is rejected");
}

}

int main() {
  Samples();
  return outshine::Test::Report();
}
