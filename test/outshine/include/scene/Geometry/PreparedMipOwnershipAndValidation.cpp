#include "Check.h"
#include <scene/Geometry.h>
#include <algorithm>
#include <array>
#include <vector>

using namespace outshine;
using outshine::Test::Report;

int main() {
  CHECK(LowerImageMipBytes(5, 3) == 12 && LowerImageMipBytes(1, 7) == 16 &&
            LowerImageMipBytes(1, 1) == 0 && !LowerImageMipBytes(0, 1),
        "lower levels halve each axis independently and contain no duplicate base bytes");
  Geometry geometry;
  const std::array<uint8_t, 16> base{};
  ImageMipData lower;
  lower[static_cast<size_t>(ImageMipKind::Colour)].emplace(4, 127);
  const auto image = geometry.addImage(
      {.WidthPx = 2, .HeightPx = 2, .Rgba = base, .LowerMips = ViewImageMips(lower)});
  CHECK(image && geometry.imageAt(0).valid(), "complete prepared levels are accepted");
  if (!image) { return Report(); }
  const auto stored = geometry.imageAt(0);
  CHECK(!stored.LowerMips[0] && stored.LowerMips[1] && !stored.LowerMips[2],
        "missing interpretations remain distinct from complete levels");
  const auto bytes = geometry.storageBytes();
  lower[1]->pop_back();
  const auto invalid = geometry.addImage(
      {.WidthPx = 2, .HeightPx = 2, .Rgba = base, .LowerMips = ViewImageMips(lower)});
  CHECK(!invalid && invalid.error() == GeometryImageError::InvalidMipChain &&
            geometry.images() == 1 && geometry.storageBytes() == bytes &&
            geometry.imageAt(0).LowerMips[1]->front() == 127,
        "incomplete lower levels leave existing native images unchanged");
  lower = {};
  lower[2].emplace();
  const auto pixel = geometry.addImage({.WidthPx = 1,
                                        .HeightPx = 1,
                                        .Rgba = std::span(base).first(4),
                                        .LowerMips = ViewImageMips(lower)});
  CHECK(pixel && geometry.imageAt(*pixel).LowerMips[2] &&
            geometry.imageAt(*pixel).LowerMips[2]->empty(),
        "an explicitly prepared one-texel image has a complete empty lower chain");
  return Report();
}
