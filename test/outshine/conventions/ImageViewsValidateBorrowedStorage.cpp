#include <array>
#include <limits>
#include <scene/Texture.h>
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::array<uint8_t, 8> pixels{};
  static_assert(!ImageView{}.valid());
  static_assert(!SurfaceMap{}.bound());
  static_assert(Sampler{} == Sampler{});
  CHECK((ImageView{.WidthPx = 2, .HeightPx = 1, .Rgba = pixels}.valid()),
        "a complete borrowed RGBA row is valid");
  CHECK((ImageView{.WidthPx = 1, .HeightPx = 2, .Rgba = pixels}.valid()),
        "multiple tightly packed rows are valid");
  for (int dimension : {0, -1, std::numeric_limits<int>::max()}) {
    CHECK(!(ImageView{.WidthPx = dimension, .HeightPx = dimension, .Rgba = pixels}.valid()),
          "invalid or unbacked extreme dimensions are rejected without pixel access");
  }
  for (size_t size = 0; size < pixels.size(); ++size) {
    CHECK(!(ImageView{
              .WidthPx = 2, .HeightPx = 1, .Rgba = std::span<const uint8_t>(pixels).first(size)}
                .valid()),
          "every truncated RGBA row is rejected");
  }
  CHECK((ImageView{.WidthPx = 1, .HeightPx = 1, .Rgba = pixels}.valid()),
        "a borrowed view may contain bytes after its declared image");
  CHECK((SurfaceMap{.Image = 42}.bound()),
        "bound reports assignment without pretending to validate the owner");
  return Report();
}
