#include "OsmField.h"
#include "OsmVector.h"
#include "../mvt/WireFixture.h"
#include "Check.h"
#include <algorithm>
#include <array>

namespace {
using outshine::Test::Mvt::Append;
using outshine::Test::Mvt::Bytes;

Bytes Tile(std::span<const Bytes> features) {
  Bytes layer{0x0a, 1, 'x', 0x78, 2, 0x28, 64};
  for (const auto &feature : features) { Append(layer, 0x12, feature); }
  Bytes tile;
  Append(tile, 0x1a, layer);
  return tile;
}
}

int main() {
  using namespace outshine::Ground;
  using namespace outshine::Test;
  const std::array<Bytes, 3> features{{{0x18, 0, 0x22, 1, 3},
                                       {0x18, 1, 0x22, 3, 9, 64, 64},
                                       {0x18, 1, 0x22, 5, 17, 32, 64, 64, 0}}};
  const std::array<std::string, 1> layers{"x"};
  OsmField native(0, layers);
  const auto result = native.Accept(0, 0, Tile(features));
  CHECK(result && *result == 2 && native.Features().size() == 2,
        "unknown experimental feature skipped, both point features retained");
  CHECK(std::ranges::equal(native.Points(), std::array<double, 6>{0, 0, 0, -90, 0, 90}),
        "single and multiple points retain independent analytical equator coordinates");
  CHECK(native.Rings().size() == 2 && native.Rings()[0].Count == 1 && native.Rings()[1].Count == 2,
        "point coordinate ranges remain reachable from native geometry");
  if (native.Features().size() == 2) {
    const auto &point = native.Features()[0];
    const auto &multiple = native.Features()[1];
    CHECK(point.Type == 1 && point.MinLat == 0 && point.MaxLat == 0 && point.MinLon == 0 &&
              point.MaxLon == 0,
          "single point has a degenerate valid bound");
    CHECK(multiple.Type == 1 && multiple.MinLat == 0 && multiple.MaxLat == 0 &&
              multiple.MinLon == -90 && multiple.MaxLon == 90,
          "multipoint bound encloses its points without changing feature type");
  }
  for (const auto &feature : std::vector<Bytes>{{}, {0x18, 0}, {0x22, 0}, {0x22, 3, 9, 0, 0}}) {
    const std::array<Bytes, 1> missing{feature};
    OsmVector decoded;
    const auto parsed = decoded.Parse(Tile(missing), "x");
    CHECK(!parsed && parsed.error() == OsmVector::ParseError::InvalidTile,
          "missing type or geometry field is invalid even for default UNKNOWN");
  }
  return Report();
}
