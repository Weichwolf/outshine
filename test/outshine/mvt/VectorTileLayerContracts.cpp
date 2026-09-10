#include "OsmVector.h"
#include "WireFixture.h"
#include "Check.h"
#include <limits>

namespace {
using outshine::Test::Mvt::Append;
using outshine::Test::Mvt::Bytes;

Bytes Tile(Bytes header, std::span<const uint8_t> tags) {
  Bytes feature{0x18, 2, 0x22, 6, 9, 1, 0, 10, 4, 2};
  Append(feature, 0x12, tags);
  Append(header, 0x12, feature);
  Append(header, 0x1a, Bytes{'k'});
  Append(header, 0x22, Bytes{0x0a, 1, 'v'});
  Bytes tile;
  Append(tile, 0x1a, header);
  return tile;
}
}

int main() {
  using namespace outshine::Ground;
  using namespace outshine::Test;
  const Bytes valid{0x0a, 1, 'x', 0x78, 2, 0x28, 64};
  for (const auto &header :
       std::vector<Bytes>{{0x0a, 1, 'x', 0x28, 64},
                          {0x0a, 1, 'x', 0x78, 2},
                          {0x0a, 1, 'x', 0x78, 2, 0x28, 0},
                          {0x0a, 1, 'x', 0x78, 3, 0x28, 64},
                          {0x0a, 1, 'x', 0x78, 0, 0x28, 64},
                          {0x0a, 1, 'x', 0x78, 0x82, 0x80, 0x80, 0x80, 0x10, 0x28, 64},
                          {0x0a, 1, 'x', 0x78, 2, 0x28, 0x80, 0x80, 0x80, 0x80, 8},
                          {0x0a, 1, 'x', 0x78, 2, 0x28, 0xc0, 0x80, 0x80, 0x80, 0x10}}) {
    const auto tile = Tile(header, Bytes{0, 0});
    OsmVector decoded;
    CHECK(!decoded.Parse(tile, "x"), "invalid layer header rejected");
  }
  for (const auto &tags : std::vector<Bytes>{{0}, {0, 0, 0}, {1, 0}, {0, 1}}) {
    const auto tile = Tile(valid, tags);
    OsmVector decoded;
    CHECK(!decoded.Parse(tile, "x"), "invalid dictionary pair rejected");
  }
  for (const auto &header :
       std::vector<Bytes>{valid,
                          {0x0a, 1, 'x', 0x78, 2, 0x28, 1},
                          {0x0a, 1, 'x', 0x78, 2, 0x28, 0xff, 0xff, 0xff, 0xff, 7}}) {
    const auto tile = Tile(header, Bytes{0, 0});
    OsmVector decoded;
    CHECK(decoded.Parse(tile, "x").has_value(), "valid extent and later dictionaries accepted");
    CHECK(decoded.Features().size() == 1 && decoded.Points().size() == 4 &&
              decoded.Points()[0] == -1 && decoded.Points()[2] == 1,
          "buffered coordinates remain legal outside extent");
    if (!decoded.Features().empty()) {
      CHECK(decoded.Str(decoded.Features().front(), "k") == "v",
            "valid tag resolves later dictionary");
    }
  }
  return Report();
}
