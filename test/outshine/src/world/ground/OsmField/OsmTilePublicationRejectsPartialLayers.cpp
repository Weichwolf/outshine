#include "OsmField.h"
#include "TileGeodesy.h"
#include <cfenv>
#include <cmath>
#include <limits>
#include "test/outshine/src/world/ground/OsmVector/WireFixture.h"
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

  struct Address {
    int Zoom;
    TileAt At;
  };

  for (const auto &address : std::array<Address, 7>{{{-1, {0, 0}},
                                                     {32, {0, 0}},
                                                     {2, {-1, 0}},
                                                     {2, {0, -1}},
                                                     {2, {4, 0}},
                                                     {2, {0, 4}},
                                                     {2, {std::numeric_limits<int>::max(), 0}}}}) {
    OsmField invalid(address.Zoom, layers);
    std::feclearexcept(FE_ALL_EXCEPT);
    const auto result = invalid.Accept(address.At.X, address.At.Y, valid);
    CHECK(!result && invalid.Features().empty() && invalid.Points().empty() &&
              !invalid.Settled(address.At.X, address.At.Y),
          "invalid tile address refuses publication");
    CHECK(std::fetestexcept(FE_INVALID | FE_OVERFLOW) == 0,
          "invalid address is rejected before unsafe arithmetic");
  }
  for (const double y : {2147483647.0,
                         -2147483647.0,
                         std::numeric_limits<double>::max(),
                         -std::numeric_limits<double>::max()}) {
    std::feclearexcept(FE_ALL_EXCEPT);
    const auto geo = TileFracToGeo({.X = 0.5, .Y = y}, 0);
    CHECK(geo.LatitudeDeg == (y > 0 ? -90.0 : 90.0) && geo.LongitudeDeg == 0,
          "large finite inverse Mercator ordinates round to analytical pole");
    CHECK(std::fetestexcept(FE_INVALID | FE_OVERFLOW) == 0,
          "inverse Mercator approaches poles without intermediate overflow");
  }
  CHECK_NEAR(TileFracToGeo({.X = 0.5, .Y = 0.5}, 0).LatitudeDeg,
             0.0,
             1e-12,
             "degrees",
             "Mercator equator");
  CHECK_NEAR(TileFracToGeo({.X = 0.5, .Y = 0}, 0).LatitudeDeg,
             85.0511287798066,
             1e-12,
             "degrees",
             "Mercator northern tile boundary");
  return Report();
}
