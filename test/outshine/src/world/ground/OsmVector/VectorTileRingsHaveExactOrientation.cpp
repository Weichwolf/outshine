#include "OsmVector.h"
#include "WireFixture.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <limits>

namespace {
using outshine::Test::Mvt::Append;
using outshine::Test::Mvt::Bytes;

struct Point {
  int32_t X, Y;
};

using Ring = std::vector<Point>;

void Word(Bytes &bytes, uint32_t word) {
  while (word >= 128) {
    bytes.push_back(static_cast<uint8_t>((word & 127u) | 128u));
    word >>= 7u;
  }
  bytes.push_back(static_cast<uint8_t>(word));
}

uint32_t Delta(int64_t value) {
  assert(value >= -int64_t{2147483647} && value <= 2147483647);
  return static_cast<uint32_t>(value >= 0 ? value * 2 : -value * 2 - 1);
}

Bytes Tile(std::span<const Ring> rings) {
  Bytes geometry;
  Point cursor{0, 0};
  for (const auto &ring : rings) {
    Word(geometry, 9);
    for (size_t i = 0; i < ring.size(); ++i) {
      if (i == 1) { Word(geometry, (static_cast<uint32_t>(ring.size() - 1) << 3u) | 2u); }
      Word(geometry, Delta(int64_t{ring[i].X} - cursor.X));
      Word(geometry, Delta(int64_t{ring[i].Y} - cursor.Y));
      cursor = ring[i];
    }
    Word(geometry, 15);
  }
  Bytes feature{0x18, 3};
  Append(feature, 0x22, geometry);
  Bytes layer{0x0a, 1, 'x', 0x78, 2, 0x28, 0x80, 0x20};
  Append(layer, 0x12, feature);
  Bytes tile;
  Append(tile, 0x1a, layer);
  return tile;
}

Ring LargeSquare(int32_t radius) {
  return {{-radius, -radius},
          {0, -radius},
          {radius, -radius},
          {radius, 0},
          {radius, radius},
          {0, radius},
          {-radius, radius},
          {-radius, 0}};
}
}

int main() {
  using namespace outshine::Ground;
  using namespace outshine::Test;
  for (const int32_t base : {0,
                             1000000000,
                             std::numeric_limits<int32_t>::max() - 8,
                             std::numeric_limits<int32_t>::min() + 8}) {
    const Ring outer{{base, base}, {base + 4, base}, {base + 4, base + 4}, {base, base + 4}};
    const Ring inner{
        {base + 1, base + 1}, {base + 1, base + 2}, {base + 2, base + 2}, {base + 2, base + 1}};
    const std::array<Ring, 2> rings{outer, inner};
    OsmVector decoded;
    CHECK(decoded.Parse(Tile(rings), "x").has_value() && decoded.Rings().size() == 2 &&
              decoded.Rings()[0].Exterior && !decoded.Rings()[1].Exterior,
          "translation preserves exact square and hole orientation");
  }
  auto inner = LargeSquare(1900000000);
  std::ranges::reverse(inner);
  const std::array<Ring, 2> large{LargeSquare(2000000000), inner};
  OsmVector decoded;
  CHECK(decoded.Parse(Tile(large), "x").has_value() && decoded.Rings().size() == 2 &&
            decoded.Rings()[0].Exterior && !decoded.Rings()[1].Exterior,
        "positive and negative twice-areas beyond uint64 range retain their signs");
  const std::array<Ring, 1> zero{Ring{{0, 0}, {1, 1}, {2, 2}}};
  CHECK(!decoded.Parse(Tile(zero), "x"), "zero-area degenerate ring rejected");
  const std::array<Ring, 1> orphan{Ring{{0, 0}, {0, 1}, {1, 0}}};
  CHECK(!decoded.Parse(Tile(orphan), "x"), "interior ring cannot precede an exterior ring");
  return Report();
}
