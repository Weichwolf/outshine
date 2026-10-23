#include "Check.h"
#include "UrbanAreaPrisms.h"
#include <array>
#include <limits>
#include <optional>

namespace {
using namespace outshine;
using namespace outshine::Test;
namespace Osm = Generators::Osm;
}

int main() {
  Osm::UrbanArea area;
  area.Columns = area.Rows = 2;
  area.Tiers = {1, 0, 2, 4};
  std::array<std::optional<double>, 4> heights{10., {}, -15., 1200.};
  std::array inputs{
      Osm::UrbanPrismInput{.Range = {.Columns = 2, .Rows = 2},
                           .GroundAslM = Osm::UrbanGroundHeights{heights.data(), 2, 2}}};
  auto result = Osm::PrepareUrbanPrisms(area, inputs, 3);
  CHECK(!result && result.error() == Osm::UrbanPrismError::OverBudget,
        "the dense sample budget includes empty cells and rejects before allocation");
  heights[0] = std::numeric_limits<double>::quiet_NaN();
  result = Osm::PrepareUrbanPrisms(area, inputs, 4);
  CHECK(!result && result.error() == Osm::UrbanPrismError::MissingTerrain,
        "nonfinite published height cannot silently become a sea-level building");
  heights[0].reset();
  result = Osm::PrepareUrbanPrisms(area, inputs, 4);
  CHECK(!result && result.error() == Osm::UrbanPrismError::MissingTerrain,
        "each occupied cell requires an explicitly supplied terrain height");
  heights[0] = 0.;
  area.Tiers[0] = 255;
  result = Osm::PrepareUrbanPrisms(area, inputs, 4);
  CHECK(!result && result.error() == Osm::UrbanPrismError::InvalidRequest,
        "invalid source tiers are rejected before the lot accessor's internal assertions");
  area.Tiers.assign(4, 0);
  inputs[0].GroundAslM = Osm::UrbanGroundHeights{};
  result = Osm::PrepareUrbanPrisms(area, inputs, 4);
  CHECK(result && result->Geometry.Samples->size() == 4,
        "an explicitly empty patch needs no synthetic terrain source");
  inputs[0].Range.Column = 2;
  result = Osm::PrepareUrbanPrisms(area, inputs, 4);
  CHECK(!result && result.error() == Osm::UrbanPrismError::InvalidRequest,
        "selection cannot address cells outside the ready source field");
  result = Osm::PrepareUrbanPrisms(area, {}, 0);
  CHECK(result && result->Geometry.Patches.empty() && result->Placements.empty(),
        "no selected demand creates no patches or placements");
  return Report();
}
