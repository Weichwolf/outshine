#include <array>

#include "Check.h"
#include "TileWatermark.h"

int main() {
  using namespace outshine::Ground;
  using namespace outshine::Test;
  const std::array<OsmField::Feature, 2> features{{{.Tile = 0}, {.Tile = 1}}};
  const std::array<OsmField::Tile, 2> tiles{{{.Z = 14, .X = 0, .Y = 1}, {.Z = 14, .X = 1, .Y = 1}}};
  TileWatermark mark;
  const TileWatermark::Next centre =
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
  return Report();
}
