#include "Check.h"
#include "shade/ImageMips.h"
#include "math/Srgb.h"
#include <array>
#include <cmath>
#include <vector>

using namespace outshine;
using outshine::Test::Report;

int main() {
  const std::array<uint8_t, 16> checker{
      0, 0, 0, 0, 255, 255, 255, 128, 255, 255, 255, 128, 0, 0, 0, 0};
  const ImageView image{.WidthPx = 2, .HeightPx = 2, .Rgba = checker};
  const auto linear = Core::PrepareImageMips(image, ImageMipKind::Linear);
  const auto colour = Core::PrepareImageMips(image, ImageMipKind::Colour);
  CHECK(linear && *linear == std::vector<uint8_t>({128, 128, 128, 64}),
        "linear data retains channel and coverage averages");
  CHECK(colour && *colour == std::vector<uint8_t>({188, 188, 188, 64}),
        "colour integrates in linear light while coverage stays linear");
  const std::array<uint8_t, 8> normals{255, 128, 128, 0, 0, 127, 127, 255};
  const auto normal =
      Core::PrepareImageMips({.WidthPx = 2, .HeightPx = 1, .Rgba = normals}, ImageMipKind::Normal);
  CHECK(normal && normal->size() == 4 && normal->back() == 0,
        "opposing directions cancel despite unrelated source alpha");
  std::array<uint8_t, 60> edge{};
  for (size_t at = 0; at < edge.size(); at += 4) { edge[at + 3] = 255; }
  edge[56] = edge[57] = edge[58] = 255;
  const auto odd =
      Core::PrepareImageMips({.WidthPx = 5, .HeightPx = 3, .Rgba = edge}, ImageMipKind::Linear);
  CHECK(odd && odd->size() == 12 && (*odd)[8] == 17 && (*odd)[9] == 17 && (*odd)[10] == 17 &&
            (*odd)[11] == 255,
        "odd edge area survives to the final texel without quantized feedback");
  const auto pixel = Core::PrepareImageMips(
      {.WidthPx = 1, .HeightPx = 1, .Rgba = std::span(edge).first(4)}, ImageMipKind::Colour);
  CHECK(pixel && pixel->empty(), "a base texel needs no duplicate prepared storage");
  CHECK(!Core::PrepareImageMips({}, ImageMipKind::Colour) &&
            !Core::PrepareImageMips(image, static_cast<ImageMipKind>(3)),
        "invalid inputs cannot produce apparently complete mip chains");
  for (int code = 0; code < 256; ++code) {
    const float value = ColourSpace::LinearFromSrgb(static_cast<float>(code) / 255.0f);
    CHECK(std::lround(ColourSpace::SrgbFromLinear(value) * 255.0f) == code,
          "direct base-byte upload preserves every previous sRGB roundtrip code");
  }
  return Report();
}
