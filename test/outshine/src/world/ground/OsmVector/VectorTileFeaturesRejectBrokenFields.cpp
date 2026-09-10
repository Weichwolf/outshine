#include "OsmVector.h"
#include "WireFixture.h"
#include "Check.h"
#include <algorithm>
#include <array>

namespace {
using outshine::Test::Mvt::Append;
using outshine::Test::Mvt::Bytes;
const Bytes kLine{0x12, 2, 0, 0, 0x18, 2, 0x22, 6, 9, 0, 0, 10, 2, 2};

Bytes Tile(std::span<const Bytes> features, std::span<const uint8_t> tail = {}) {
  Bytes layer{0x0a, 1,   'x', 0x78, 2, 0x28, 0x80, 0x20, 0x1a, 4,   'k',
              'i',  'n', 'd', 0x22, 6, 0x0a, 4,    'r',  'o',  'a', 'd'};
  for (const auto &feature : features) { Append(layer, 0x12, feature); }
  layer.insert(layer.end(), tail.begin(), tail.end());
  Bytes tile;
  Append(tile, 0x1a, layer);
  return tile;
}
}

int main() {
  using namespace outshine::Ground;
  using namespace outshine::Test;
  for (const auto &bad : std::vector<Bytes>{{},
                                            {0x18, 0},
                                            {0x22, 0},
                                            {0x22, 3, 9, 0, 0},
                                            {0x12, 1, 0x80},
                                            {0x22, 1, 0x80},
                                            {0x12, 5, 0},
                                            {0x22, 5, 0},
                                            {0x18, 0x80},
                                            {0x18, 4},
                                            {0x18, 0x82, 0x80, 0x80, 0x80, 0x10},
                                            {0x12, 5, 0x80, 0x80, 0x80, 0x80, 0x10},
                                            {0x22, 5, 0x80, 0x80, 0x80, 0x80, 0x10},
                                            {0x10, 0x80},
                                            {0x20, 0x80},
                                            {0x80},
                                            {0},
                                            {0x41, 0}}) {
    const std::array<Bytes, 2> features{kLine, bad};
    const auto tile = Tile(features);
    OsmVector decoded;
    const auto result = decoded.Parse(tile, "x");
    CHECK(!result && result.error() == OsmVector::ParseError::InvalidTile,
          "late malformed feature refuses the present layer");
  }
  for (const auto &tail : std::vector<Bytes>{{0x80}, {0}, {0x28, 0x80}}) {
    const std::array<Bytes, 1> features{kLine};
    const auto tile = Tile(features, tail);
    OsmVector decoded;
    CHECK(!decoded.Parse(tile, "x"), "truncated layer field refuses parsing");
  }
  for (const auto &feature : std::vector<Bytes>{
           kLine,
           {0x10, 0, 0x10, 0, 0x18, 2, 0x20, 9, 0x20, 0, 0x20, 0, 0x20, 10, 0x20, 2, 0x20, 2},
           {0x12, 1, 0, 0x12, 1, 0, 0x18, 2, 0x22, 3, 9, 0, 0, 0x22, 3, 10, 2, 2},
           {0x12, 1, 0, 0x10, 0, 0x18, 2, 0x22, 3, 9, 0, 0, 0x20, 10, 0x20, 2, 0x20, 2}}) {
    const std::array<Bytes, 1> features{feature};
    const auto tile = Tile(features);
    OsmVector decoded;
    CHECK(decoded.Parse(tile, "x") && decoded.Features().size() == 1,
          "packed, unpacked and segmented forms are valid");
    const std::array<int32_t, 4> points{0, 0, 1, 1};
    CHECK(std::ranges::equal(decoded.Points(), points) && decoded.Rings().size() == 1 &&
              decoded.Rings().front().Count == 2,
          "all representations decode the analytical two-point line");
    if (!decoded.Features().empty()) {
      CHECK(decoded.Str(decoded.Features().front(), "kind") == "road",
            "tag segments concatenate in encounter order");
    }
  }
  return Report();
}
