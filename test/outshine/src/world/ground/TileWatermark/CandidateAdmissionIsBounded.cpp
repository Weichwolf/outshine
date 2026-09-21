#include <array>
#include <cstddef>

#include "Check.h"
#include "TileWatermark.h"

int main() {
  using namespace outshine::Ground;
  using namespace outshine::Test;
  const std::array<OsmField::Feature, 3> features{{{.Tile = 0}, {.Tile = 1}, {.Tile = 2}}};
  const std::array<OsmField::Tile, 3> tiles{
      {{.Z = 14, .X = 1, .Y = 1}, {.Z = 14, .X = 2, .Y = 1}, {.Z = 14, .X = 3, .Y = 1}}};
  TileWatermark mark;
  size_t attempts = 0;
  const TileWatermark::Next next =
      mark.Ask(features,
               tiles,
               {.CentreX = 1, .CentreY = 1, .Rings = kEveryRing, .CandidatesMost = 1},
               [&attempts](size_t, size_t) {
                 ++attempts;
                 return false;
               });
  CHECK(!next.Found && attempts == 1,
        "candidate admission stops before unrelated dependency work is started");
  return Report();
}
