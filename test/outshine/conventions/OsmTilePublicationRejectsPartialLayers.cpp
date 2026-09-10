#include "OsmField.h"
#include "../mvt/WireFixture.h"
#include "Check.h"
#include <array>

namespace {
using outshine::Test::Mvt::Append;
using outshine::Test::Mvt::Bytes;

Bytes Layer(char name, bool broken) {
  Bytes layer{0x0a, 1, static_cast<uint8_t>(name), 0x78, 2, 0x28, 64};
  const Bytes feature =
      broken ? Bytes{0x18, 2, 0x22, 1, 9} : Bytes{0x18, 2, 0x22, 6, 9, 0, 0, 10, 2, 2};
  Append(layer, 0x12, feature);
  return layer;
}

Bytes Tile(bool broken) {
  Bytes tile;
  Append(tile, 0x1a, Layer('x', false));
  Append(tile, 0x1a, Layer('y', broken));
  return tile;
}
}

int main() {
  using namespace outshine::Ground;
  using namespace outshine::Test;
  const std::array<std::string, 2> layers{"x", "y"};
  OsmField field(2, layers);
  const auto valid = Tile(false);
  const auto initial = field.Accept(1, 1, valid);
  CHECK(static_cast<bool>(initial), "initial native tile accepted");
  const auto *points = field.Points().data();
  const auto featureCount = field.Features().size();
  const auto pointCount = field.Points().size();
  const auto ringCount = field.Rings().size();
  const auto extent = field.Extent();
  const auto generation = field.Generation();
  const auto rejected = field.Accept(2, 1, Tile(true));
  CHECK(!rejected, "late malformed layer rejects whole tile");
  CHECK(!field.Settled(2, 1) && field.TileIndex(2, 1) == -1,
        "failed tile publishes neither completion nor tile record");
  CHECK(field.Points().data() == points && field.Features().size() == featureCount &&
            field.Points().size() == pointCount && field.Rings().size() == ringCount &&
            field.Extent() == extent && field.Generation() == generation,
        "failed tile preserves existing native storage and content");
  const auto corrected = field.Accept(2, 1, valid);
  CHECK(static_cast<bool>(corrected) && field.Settled(2, 1) &&
            field.Features().size() == featureCount * 2,
        "corrected retry publishes each feature exactly once");
  Bytes missing;
  Append(missing, 0x1a, Layer('x', false));
  const auto accepted = field.Accept(3, 1, missing);
  CHECK(static_cast<bool>(accepted) && field.Settled(3, 1) &&
            field.Features().size() == featureCount * 2 + 1,
        "missing optional layer does not reject valid tile");
  return Report();
}
