#include "ByteArchive.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  ByteWriter out(32);
  CHECK(out.Number(uint8_t{0xa5}) && out.Number(uint32_t{0}) && out.Number(uint8_t{0x5a}),
        "a length field is embedded between adjacent bytes");
  CHECK(out.NumberAt(1, uint32_t{0x01020304}), "an unaligned existing length can be patched");
  constexpr std::array<uint8_t, 6> expected{0xa5, 4, 3, 2, 1, 0x5a};
  CHECK(std::ranges::equal(out.Bytes(), expected),
        "patching preserves canonical little-endian bytes");
  CHECK(!out.NumberAt(3, uint32_t{1}) && !out.NumberAt(6, uint8_t{1}) &&
            !out.NumberAt(std::numeric_limits<size_t>::max(), uint64_t{1}),
        "reserved capacity and overflowing offsets never admit unwritten lengths");
  CHECK(std::ranges::equal(out.Bytes(), expected), "failed patches leave every byte unchanged");
  return Report();
}
