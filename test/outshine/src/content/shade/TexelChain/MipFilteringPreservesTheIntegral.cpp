#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "TexelChain.h"
#include "Check.h"

int main() {
  using namespace outshine::Core;
  using namespace outshine::Test;
  const std::array<float, 16> checker{0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0};
  std::vector<float> reduced;
  const auto size = HalveInPlace(checker, {.WidthPx = 2, .HeightPx = 2}, reduced, TexelKind::Value);
  CHECK(size.WidthPx == 1 && size.HeightPx == 1, "two-by-two reduces to one texel");
  CHECK(std::ranges::all_of(reduced, [](float v) { return v == 0.5f; }),
        "equal black and white areas integrate to one half in every channel");
  for (const Texels extent : {Texels{.WidthPx = 3, .HeightPx = 1},
                              Texels{.WidthPx = 1, .HeightPx = 3},
                              Texels{.WidthPx = 5, .HeightPx = 3},
                              Texels{.WidthPx = 7, .HeightPx = 5}}) {
    std::vector<float> edge(static_cast<size_t>(extent.WidthPx) * extent.HeightPx * 4u, 0.0f);
    for (uint32_t y = 0; y < extent.HeightPx; ++y) {
      for (uint32_t x = 0; x < extent.WidthPx; ++x) {
        for (size_t c = 0; c < 4; ++c) {
          edge[(static_cast<size_t>(y) * extent.WidthPx + x) * 4 + c] =
              (x + 1 == extent.WidthPx && y + 1 == extent.HeightPx) ? 1.0f : 0.0f;
        }
      }
    }
    Texels current = extent;
    while (current.WidthPx > 1 || current.HeightPx > 1) {
      current = HalveInPlace(edge, current, reduced, TexelKind::Value);
      edge.swap(reduced);
    }
    const float integral = 1.0f / static_cast<float>(extent.WidthPx * extent.HeightPx);
    CHECK(std::ranges::all_of(edge,

                              [integral](float v) { return std::abs(v - integral) < 1e-6f; }),
          "odd right and bottom edge areas survive the entire mip chain");
  }
  const std::array<float, 16> normal{
      0.5, 0.5, 1, 1, 0.5, 0.5, 1, 1, 0.5, 0.5, 1, 1, 0.5, 0.5, 1, 1};
  (void)HalveInPlace(normal, {.WidthPx = 2, .HeightPx = 2}, reduced, TexelKind::Direction);
  CHECK(reduced == std::vector<float>({0.5, 0.5, 1, 1}), "constant unit normal survives filtering");
  const std::array<float, 16> varied{
      1, 0.5, 0.5, 1, 1, 0.5, 0.5, 1, 0, 0.5, 0.5, 1, 0.5, 0.5, 1, 1};
  (void)HalveInPlace(varied, {.WidthPx = 4, .HeightPx = 1}, reduced, TexelKind::Direction);
  std::vector<float> final;
  (void)HalveInPlace(reduced, {.WidthPx = 2, .HeightPx = 1}, final, TexelKind::Direction);
  CHECK_NEAR((final[0] * 2.0f - 1.0f) * final[3],
             0.25f,
             1e-6f,
             "normal x moment",
             "mip partition preserves the source direction integral");
  CHECK_NEAR((final[2] * 2.0f - 1.0f) * final[3],
             0.25f,
             1e-6f,
             "normal z moment",
             "mip partition preserves the source direction integral");
  const std::array<float, 8> cancelling{1, 0.5, 0.5, 1, 0, 0.5, 0.5, 1};
  (void)HalveInPlace(cancelling, {.WidthPx = 2, .HeightPx = 1}, reduced, TexelKind::Direction);
  CHECK(reduced == std::vector<float>({0.5f, 0.5f, 1.0f, 0.0f}),
        "zero resultant keeps a finite neutral direction and zero confidence");
  return Report();
}
