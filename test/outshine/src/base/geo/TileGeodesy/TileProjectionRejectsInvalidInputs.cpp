#include "TileGeodesy.h"
#include "Check.h"
#include <array>
#include <cfenv>
#include <cmath>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  for (const Geo geo : std::array<Geo, 8>{
           {{nan, 0}, {0, nan}, {inf, 0}, {0, inf}, {181, 0}, {-181, 0}, {0, 91}, {0, -91}}}) {
    std::feclearexcept(FE_ALL_EXCEPT);
    const auto index = TileIndex::Of(geo, 14);
    CHECK(index.Where() == TileIndex::State::InvalidInput && !index.Tile(),
          "invalid canonical coordinates have no tile and are distinct from absent coverage");
    CHECK(std::fetestexcept(FE_INVALID) == 0,
          "invalid coordinates are rejected before integer conversion");
  }
  for (const int zoom :
       {-1, 33, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
    std::feclearexcept(FE_ALL_EXCEPT);
    const auto index = TileIndex::Of({}, zoom);
    CHECK(index.Where() == TileIndex::State::InvalidInput && !index.Tile(),
          "zoom outside uint32 tile grid capacity is refused");
    CHECK(std::fetestexcept(FE_INVALID | FE_OVERFLOW) == 0,
          "zoom rejected before exponential arithmetic");
  }
  for (const double latitude : {-90.0, 90.0}) {
    const auto index = TileIndex::Of({.LatitudeDeg = latitude}, 14);
    CHECK(index.Where() == TileIndex::State::OutsideMercatorBand && !index.Tile(),
          "valid geographic poles are outside Mercator coverage");
  }
  for (const int zoom : {0, 1, 14, 32}) {
    const uint64_t count = uint64_t{1} << zoom;
    const auto centre = TileIndex::Of({}, zoom).Tile();
    CHECK(centre && centre->X == count / 2 && centre->Y == count / 2,
          "equator and prime meridian map to the analytical centre cell");
    const auto west = TileIndex::Of({.LongitudeDeg = -180}, zoom).Tile();
    const auto east = TileIndex::Of({.LongitudeDeg = 180}, zoom).Tile();
    CHECK(west && east && west->X == 0 && east->X == count - 1,
          "closed longitude endpoints select the first and last columns");
    const auto north = TileIndex::Of({.LatitudeDeg = kMercatorLatMaxDeg}, zoom).Tile();
    const auto south = TileIndex::Of({.LatitudeDeg = -kMercatorLatMaxDeg}, zoom).Tile();
    CHECK(north && south && north->Y == 0 && south->Y == count - 1,
          "Mercator band boundaries select first and last rows");
  }
  const auto ignoredHeight = TileIndex::Of({.HeightM = nan}, 1).Tile();
  CHECK(ignoredHeight && ignoredHeight->X == 1 && ignoredHeight->Y == 1,
        "tile choice depends only on angular coordinates");
  const auto edge = TileBounds({.Zoom = 32,
                                .X = std::numeric_limits<uint32_t>::max(),
                                .Y = std::numeric_limits<uint32_t>::max()});
  CHECK(edge.MaxLonDeg == 180 && edge.MinLonDeg < edge.MaxLonDeg && edge.MinLonDeg > 179,
        "last column ends at east dateline without integer wrap");
  CHECK(edge.MinLatDeg < edge.MaxLatDeg && edge.MaxLatDeg < -85 &&
            edge.MinLatDeg >= -kMercatorLatMaxDeg - 1e-12,
        "last row remains ordered at southern Mercator boundary");
  return Report();
}
