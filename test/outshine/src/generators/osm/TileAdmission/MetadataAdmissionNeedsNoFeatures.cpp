#include "Check.h"
#include "TileAdmission.h"
#include <array>
#include <cstddef>

int main() {
  using namespace outshine::Generators::Osm;
  using namespace outshine::Test;
  const std::array<OsmField::Tile, 3> tiles{
      {{.Z = 14, .X = 12, .Y = 10, .FirstFeature = 400000000, .FeatureCount = 100000000},
       {.Z = 14, .X = 10, .Y = 10, .FirstFeature = 600000000, .FeatureCount = 200000000},
       {.Z = 14, .X = 10, .Y = 11, .FirstFeature = 800000000, .FeatureCount = 0}}};
  TileAdmission admission;
  size_t attempts = 0;
  const auto nearest = admission.Ask(
      tiles, {.CentreX = 10, .CentreY = 10, .CandidatesMost = 1}, [&](size_t from, size_t to) {
        ++attempts;
        return from == 600000000 && to == 800000000;
      });
  CHECK(nearest.Found && nearest.Tile == 1 && attempts == 1,
        "metadata orders and admits a native work range without opening any feature array");
  admission.Take(1);
  CHECK(!admission.Done(tiles) && admission.AcceptedWithin(tiles, 10, 10, 0),
        "empty metadata ranges require no work and local acceptance stays separate");
  const auto next =
      admission.Ask(tiles, {.CentreX = 10, .CentreY = 10}, [](size_t, size_t) { return true; });
  CHECK(next.Found && next.Tile == 0 && next.From == 400000000 && next.To == 500000000,
        "native range length changes no admission work or coverage semantics");
  admission.Take(0);
  CHECK(admission.Done(tiles), "all nonempty native work ranges are reserved");
  admission.Release(1);
  CHECK(!admission.Done(tiles) && !admission.AcceptedWithin(tiles, 10, 10, 0),
        "release restores missing coverage without rebuilding a feature scan cursor");
  return Report();
}
