#include "Check.h"
#include "StructureInput.h"

#include <utility>

int main() {
  using namespace outshine::Test;
  outshine::Ground::StructureFootprints footprints;
  footprints.Origin.Bounds = {.WestDeg = -1, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 1};
  footprints.LatLon = {0, 0, 0, 0.001, 0.001, 0.001, 0.001, 0};
  footprints.Rings = {{.First = 0, .Count = 4, .Exterior = true}};
  footprints.Structures.push_back(
      {.First = 0,
       .Count = 4,
       .Height = {.TopM = 13,
                  .MinimumM = 3,
                  .TopOrigin = outshine::Ground::BuildingHeightOrigin::Declared,
                  .MinimumOrigin = outshine::Ground::BuildingHeightOrigin::Declared,
                  .ConflictingLevels = false},
       .Source = {},
       .Openings = outshine::Ground::StructureOpenings::Closed});
  const auto *points = footprints.LatLon.data();
  const auto *rings = footprints.Rings.data();
  auto input = outshine::Generators::StructureInput(std::move(footprints));
  CHECK(input && input->LatLon.data() == points && input->Holes.data() == rings,
        "native geometry ownership crosses the boundary without duplicating coordinate buffers");
  CHECK(input && !input->SourceInputs.Objects && input->Structures.size() == 1 &&
            input->Structures.front().HeightM == 13 &&
            input->Structures.front().MinimumHeightM == 3 &&
            input->Structures.front().Facade == outshine::FacadeStyle::Outbuilding &&
            input->Structures.front().HeightOrigin ==
                outshine::Ground::BuildingHeightOrigin::Declared,
        "a producer needs no source archive to supply an explicit raised building");
  if (input) {
    const auto &bounds = input->Structures.front().Cell.Footprint;
    CHECK(bounds.MinLatDeg == 0 && bounds.MinLonDeg == 0 && bounds.MaxLatDeg == 0.001 &&
              bounds.MaxLonDeg == 0.001,
          "the cell carries the analytically known footprint bounds");
  }
  outshine::Ground::StructureFootprints invalid;
  invalid.Structures.push_back({.First = 1, .Count = 4, .Height = {}, .Source = {}});
  const auto refused = outshine::Generators::StructureInput(std::move(invalid));
  CHECK(!refused && refused.error() == outshine::Generators::StructureInputError::InvalidCell,
        "missing coordinate storage is refused before forming an out-of-range span");
  CHECK(outshine::Generators::StructureInput({}).has_value(),
        "a complete empty native product remains distinct from malformed geometry");
  outshine::Ground::StructureFootprints glazed;
  glazed.Origin.Bounds = {.WestDeg = -1, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 1};
  glazed.LatLon = {0, 0, 0, 0.001, 0.001, 0.001, 0.001, 0};
  glazed.Structures.push_back({.First = 0,
                               .Count = 4,
                               .Height = {.TopM = 8},
                               .Source = {},
                               .Openings = outshine::Ground::StructureOpenings::Glazed});
  const auto glassInput = outshine::Generators::StructureInput(std::move(glazed));
  CHECK(glassInput && glassInput->Structures.front().Facade == outshine::FacadeStyle::Glazing,
        "a native glazed building retains its supplied opening plan");
  return Report();
}
