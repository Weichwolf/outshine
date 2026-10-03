#include <array>
#include <cstdint>

#include "Check.h"
#include "TileWatermark.h"

int main() {
  using namespace outshine::Generators::Osm;
  using namespace outshine::Test;
  const std::array<::outshine::Generators::Osm::OsmField::Feature, 2> features{
      {{.Tile = 0}, {.Tile = 1}}};
  const std::array<::outshine::Generators::Osm::OsmField::Tile, 2> tiles{
      {{.Z = 14, .X = 0, .Y = 1}, {.Z = 14, .X = 1, .Y = 1}}};
  ::outshine::Generators::Osm::TileWatermark mark;
  const ::outshine::Generators::Osm::TileWatermark::Next centre =
      mark.Ask(features, tiles, {.CentreX = 1, .CentreY = 1, .Rings = 0}, [](size_t, size_t) {
        return true;
      });
  CHECK(centre.Found && centre.Tile == 1, "central source tile is admitted");
  mark.Take(centre.Tile);
  mark.Advance(features);
  CHECK(mark.Done(features), "skipped and accepted tiles advance the scan watermark");
  CHECK(mark.AcceptedWithin(features, tiles, 1, 1, 0), "accepted central coverage is complete");
  CHECK(!mark.AcceptedWithin(features, tiles, 1, 1, 1),
        "skipped source does not masquerade as accepted wider coverage");
  const std::array<uint32_t, 1> accepted{1};
  CHECK(mark.ReleaseUnaccepted(accepted) == 0 && mark.Takes() == 1 &&
            !mark.AcceptedWithin(features, tiles, 1, 1, 1),
        "dropping pending reservations preserves skipped and accepted tiles");
  ::outshine::Generators::Osm::TileWatermark mixed;
  mixed.Take(0);
  mixed.Take(1);
  CHECK(mixed.ReleaseUnaccepted(accepted) == 1 && mixed.Takes() == 1,
        "unaccepted reservation leaves an out-of-order accepted tile intact");
  const ::outshine::Generators::Osm::TileWatermark::Next retry = mixed.Ask(
      features, tiles, {.CentreX = 0, .CentreY = 1, .Rings = kEveryRing}, [](size_t, size_t) {
        return true;
      });
  CHECK(retry.Found && retry.Tile == 0, "released tile can be admitted again");
  return Report();
}
