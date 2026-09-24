#include "Check.h"
#include "SourcedTerrainFields.h"
#include "TileGeodesy.h"

#include <cstdint>
#include <memory>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  auto west = std::make_shared<Ground::TerrainField>(2, 2);
  auto east = std::make_shared<Ground::TerrainField>(2, 2);
  for (uint32_t row = 0; row < 2; ++row) {
    for (uint32_t column = 0; column < 2; ++column) {
      west->SetM(row, column, 10.0f);
      east->SetM(row, column, 20.0f);
    }
  }
  std::vector<SourcedTerrainFields::Entry> fields{{{.Zoom = 1, .X = 0, .Y = 0}, west},
                                                  {{.Zoom = 1, .X = 1, .Y = 0}, east}};
  const SourcedTerrainFields snapshot(fields);
  fields.clear();
  west.reset();
  east.reset();
  const Ground::Geo westGeo = Ground::TileFracToGeo({.X = 0.25, .Y = 0.25}, 1);
  const Ground::Geo eastGeo = Ground::TileFracToGeo({.X = 1.25, .Y = 0.25}, 1);
  const Ground::Geo missingGeo = Ground::TileFracToGeo({.X = 0.25, .Y = 1.25}, 1);
  const auto at = [&snapshot](Ground::Geo geo) {
    return snapshot.AslMAt(1, {.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg});
  };
  const auto westM = at(westGeo);
  const auto eastM = at(eastGeo);
  const auto missingM = at(missingGeo);
  CHECK(westM && *westM == 10.0 && eastM && *eastM == 20.0,
        "the owned source snapshot samples its original immutable fields");
  CHECK(!missingM, "an absent source tile does not borrow an adjacent field");
  return Report();
}
