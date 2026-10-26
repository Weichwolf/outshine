#include "Check.h"
#include "HalfTexels.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

int main() {
  using namespace outshine::Core;
  using outshine::Test::Report;
  constexpr Texels extent{.WidthPx = 1, .HeightPx = 1};
  constexpr std::array<float, 4> source{1, 0.5f, 0x1p-24f, 0};
  constexpr std::array<uint8_t, 8> expected{0, 0x3c, 0, 0x38, 1, 0, 0, 0};
  const auto packed = PackHalfTexels(extent, source);
  CHECK(packed && std::ranges::equal(*packed, expected),
        "binary16 stores linear fields and its smallest subnormal without gamma conversion");
  if (!packed) { return Report(); }
  std::vector<uint8_t> unaligned(packed->size() + 1);
  std::ranges::copy(*packed, unaligned.begin() + 1);
  const auto decoded = UnpackHalfTexels(extent, std::span(unaligned).subspan(1));
  CHECK(decoded && std::ranges::equal(*decoded, source),
        "unaligned native asset bytes decode without treating them as aligned float storage");
  CHECK(!PackHalfTexels(extent, std::span(source).first(3)) &&
            !UnpackHalfTexels(extent, std::span(expected).first(expected.size() - 1)),
        "both conversion directions reject incomplete source pixels");
  CHECK(!PackHalfTexels({.WidthPx = std::numeric_limits<uint32_t>::max(), .HeightPx = 1}, {}) &&
            !UnpackHalfTexels({.WidthPx = 1, .HeightPx = 0}, {}),
        "invalid pitch and extent fail before allocating or reading any pixels");
  return Report();
}
