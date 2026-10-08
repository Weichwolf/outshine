#include <array>
#include <cstddef>

#include "Check.h"
#include "TileAdmission.h"

int main() {
  using namespace outshine::Generators::Osm;
  using namespace outshine::Test;
  const std::array<::outshine::Generators::Osm::OsmField::Tile, 3> tiles{
      {{.Z = 14, .X = 1, .Y = 1, .FirstFeature = 0, .FeatureCount = 1},
       {.Z = 14, .X = 2, .Y = 1, .FirstFeature = 1, .FeatureCount = 1},
       {.Z = 14, .X = 3, .Y = 1, .FirstFeature = 2, .FeatureCount = 1}}};
  ::outshine::Generators::Osm::TileAdmission mark;
  size_t attempts = 0;
  const ::outshine::Generators::Osm::TileAdmission::Next next =
      mark.Ask(tiles,
               {.CentreX = 1, .CentreY = 1, .Rings = kEveryRing, .CandidatesMost = 1},
               [&attempts](size_t, size_t) {
                 ++attempts;
                 return false;
               });
  CHECK(!next.Found && attempts == 1,
        "candidate admission stops before unrelated dependency work is started");
  return Report();
}
