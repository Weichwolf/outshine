#include "OsmField.h"
#include "test/outshine/src/world/ground/OsmVector/WireFixture.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <string>

namespace {
using outshine::Test::Mvt::Append;
using outshine::Test::Mvt::Bytes;

Bytes Tile() {
  Bytes layer{0x0a, 1, 'x', 0x78, 2, 0x28, 64};
  Append(layer, 0x12, Bytes{0x18, 2, 0x22, 6, 9, 0, 0, 10, 2, 2});
  Bytes tile;
  Append(tile, 0x1a, layer);
  return tile;
}
}

int main() {
  using namespace outshine::Ground;
  using namespace outshine::Test;
  const std::array<std::string, 1> layers{"x"};
  OsmField forward(2, layers);
  OsmField reverse(2, layers);
  const Bytes tile = Tile();
  const auto a = forward.Accept(1, 1, tile);
  const auto b = forward.Accept(2, 1, tile);
  const auto c = reverse.Accept(2, 1, tile);
  const auto d = reverse.Accept(1, 1, tile);
  CHECK(a && b && c && d, "both arrival orders accept the same vector tiles");
  if (!a || !b || !c || !d) { return Report(); }
  CHECK(forward.Tiles().size() == reverse.Tiles().size() &&
            forward.Features().size() == reverse.Features().size() &&
            forward.Rings().size() == reverse.Rings().size(),
        "native snapshots have matching storage counts");
  bool sameTiles = forward.Tiles().size() == reverse.Tiles().size();
  for (size_t at = 0; at < forward.Tiles().size() && sameTiles; ++at) {
    const auto &left = forward.Tiles()[at];
    const auto &right = reverse.Tiles()[at];
    sameTiles = left.X == right.X && left.Y == right.Y && left.FirstFeature == right.FirstFeature &&
                left.FeatureCount == right.FeatureCount;
  }
  CHECK(sameTiles, "native tile indices follow spatial identity, not arrival order");
  CHECK(std::ranges::equal(forward.Points(), reverse.Points()),
        "feature geometry has one native order for the same source tiles");
  return Report();
}
