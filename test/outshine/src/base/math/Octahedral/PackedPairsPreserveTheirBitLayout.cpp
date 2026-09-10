#include <array>
#include <cstdint>
#include <limits>
#include "Check.h"
#include "Octahedral.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(PackedPair({-1.0f, -1.0f}) == 0u, "both negative endpoints encode as zero");
  CHECK(PackedPair({1.0f, 1.0f}) == 0xffffffffu, "both positive endpoints set every bit");
  CHECK(PackedPair({1.0f, -1.0f}) == 0xffff0000u, "first component occupies the high sixteen bits");
  CHECK(PackedPair({-1.0f, 1.0f}) == 0x0000ffffu, "second component occupies the low sixteen bits");
  CHECK(PackedPair({0.0f, 0.0f}) == 0x80008000u, "midpoints round to the upper UNORM code");
  CHECK(PackedPair({-2.0f, 2.0f}) == 0x0000ffffu, "finite out-of-range inputs saturate");
  constexpr std::array<float, 2> expected = {1.0f, -1.0f};
  CHECK(UnpackedPair(0xffff0000u) == expected,
        "decoding independently respects channel order and endpoint range");
  constexpr uint32_t largest = std::numeric_limits<uint16_t>::max();
  bool stable = true;
  for (uint32_t code = 0; code <= largest; ++code) {
    const uint32_t word = (code << 16u) | (largest - code);
    stable = stable && PackedPair(UnpackedPair(word)) == word;
  }
  CHECK(stable, "all sixteen-bit channel codes survive decode/encode in both positions");
  Covers("two UNORM16 channels in one word: endpoints, order, midpoint rounding, finite saturation "
         "and exhaustive per-channel code stability; not non-finite input or angular error");
  return Report();
}
