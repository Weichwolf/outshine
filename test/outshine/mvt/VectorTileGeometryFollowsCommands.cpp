#include "OsmVector.h"
#include "WireFixture.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <limits>

namespace {
using outshine::Test::Mvt::Append;
using outshine::Test::Mvt::Bytes;

Bytes Tile(uint8_t type, std::span<const uint32_t> words) {
  Bytes geometry;
  for (auto word : words) {
    while (word >= 128) {
      geometry.push_back(static_cast<uint8_t>((word & 127u) | 128u));
      word >>= 7u;
    }
    geometry.push_back(static_cast<uint8_t>(word));
  }
  Bytes feature{0x18, type};
  Append(feature, 0x22, geometry);
  Bytes layer{0x0a, 1, 'x', 0x78, 2, 0x28, 0x80, 0x20};
  Append(layer, 0x12, feature);
  Bytes tile;
  Append(tile, 0x1a, layer);
  return tile;
}

bool Parse(outshine::Ground::OsmVector &decoded, uint8_t type, std::span<const uint32_t> words) {
  const auto tile = Tile(type, words);
  return decoded.Parse(tile, "x").has_value();
}
}

int main() {
  using namespace outshine::Ground;
  using namespace outshine::Test;

  struct Invalid {
    uint8_t Type;
    std::vector<uint32_t> Words;
  };

  const uint32_t maxDelta = std::numeric_limits<uint32_t>::max() - 1;
  for (const auto &bad : std::vector<Invalid>{{1, {}},
                                              {1, {1}},
                                              {1, {9, 0}},
                                              {1, {9, 0, 0, 9, 2, 2}},
                                              {1, {9, 0, 0, 10, 2, 2}},
                                              {2, {}},
                                              {2, {9, 0, 0}},
                                              {2, {10, 2, 2}},
                                              {2, {17, 0, 0, 2, 2, 10, 2, 2}},
                                              {2, {9, 0, 0, 2}},
                                              {2, {9, 0, 0, 10, 0, 0}},
                                              {2, {9, 0, 0, 10, 2}},
                                              {2, {9, 0, 0, 10, 2, 2, 15}},
                                              {2, {9, 0, 0, 10, 2, 2, 10, 2, 2}},
                                              {2, {9, 0, 0, 11}},
                                              {3, {9, 0, 0, 10, 2, 2, 15}},
                                              {3, {9, 0, 0, 18, 2, 0, 0, 2}},
                                              {3, {9, 0, 0, 18, 2, 0, 0, 2, 23}},
                                              {3, {9, 0, 0, 18, 2, 0, 1, 0, 15}},
                                              {1, {9, std::numeric_limits<uint32_t>::max(), 0}},
                                              {1, {17, maxDelta, 0, 2, 0}},
                                              {1, {17, 0, maxDelta, 0, 2}},
                                              {1, {17, maxDelta - 1, 0, 3, 0}}}) {
    OsmVector decoded;
    CHECK(!Parse(decoded, bad.Type, bad.Words),
          "invalid geometry commands or coordinates rejected");
  }
  OsmVector points;
  const std::array<uint32_t, 5> multiPoint{17, 10, 14, 5, 9};
  CHECK(Parse(points, 1, multiPoint), "specification multipoint accepted");
  CHECK(std::ranges::equal(points.Points(), std::array<int32_t, 4>{5, 7, 2, 2}),
        "multipoint cursor accumulates deltas");
  OsmVector lines;
  const std::array<uint32_t, 14> multiLine{9, 4, 4, 18, 0, 16, 16, 0, 9, 17, 17, 10, 4, 8};
  CHECK(Parse(lines, 2, multiLine), "specification multiline accepted");
  CHECK(std::ranges::equal(lines.Points(),
                           std::array<int32_t, 10>{2, 2, 2, 10, 10, 10, 1, 1, 3, 5}) &&
            lines.Rings().size() == 2 && lines.Rings()[0].Count == 3 && lines.Rings()[1].Count == 2,
        "multiline retains independent paths and shared cursor");
  OsmVector polygon;
  const std::array<uint32_t, 22> rings{9, 0, 0,  26, 20, 0,  0,  20, 19, 0,  15,
                                       9, 4, 15, 26, 0,  12, 12, 0,  0,  11, 15};
  CHECK(Parse(polygon, 3, rings), "polygon with interior ring accepted");
  CHECK(polygon.Rings().size() == 2 && polygon.Rings()[0].Exterior &&
            !polygon.Rings()[1].Exterior && polygon.Points().size() == 16 &&
            polygon.Points()[8] == 2 && polygon.Points()[9] == 2,
        "ClosePath preserves cursor and hole winding");
  return Report();
}
