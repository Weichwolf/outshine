#include "OsmField.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array<std::string, 2> layers{"transportation", "building"};
  Ground::OsmField field(14, layers);
  std::array<Ground::OsmField::Declared, 2> input{{{.Layer = "transportation",
                                                    .Key = "class-0",
                                                    .Value = "road-0",
                                                    .WidthM = 6.5,
                                                    .HeightM = 2,
                                                    .Bridge = true,
                                                    .Level = 1,
                                                    .LatLon = {1, 2, 3, 4}},
                                                   {.Layer = "building",
                                                    .Key = "kind",
                                                    .Value = "house",
                                                    .HeightM = 12,
                                                    .Area = true,
                                                    .Tunnel = true,
                                                    .Level = -1,
                                                    .LatLon = {5, 6, 7, 8, 5, 8}}}};
  const auto declare = [&] { field.Declare(input, Ground::TileAt{.X = 8, .Y = 9}); };
  declare();
  const size_t warmBytes = field.HeapBytes();
  const size_t warmKeys = field.KeyCount();
  const uint64_t originalGeneration = field.Generation();
  for (int iteration = 0; iteration < 128; ++iteration) {
    declare();
    CHECK(field.HeapBytes() == warmBytes && field.KeyCount() == warmKeys,
          "identical declarations reuse their warmed storage");
    CHECK(field.Generation() == originalGeneration, "unchanged contents keep their generation");
  }
  for (int iteration = 1; iteration <= 128; ++iteration) {
    input[0].Key = "class-" + std::to_string(iteration);
    input[0].Value = "road-" + std::to_string(iteration);
    declare();
    CHECK(field.HeapBytes() == warmBytes && field.KeyCount() == warmKeys,
          "replaced keys and values do not accumulate dead interned content");
    CHECK(field.Features().size() == 2 && field.Rings().size() == 2 && field.Points().size() == 10,
          "only current feature geometry remains");
    if (field.Features().size() != 2) { continue; }
    const auto &road = field.Features()[0];
    const auto &building = field.Features()[1];
    CHECK(field.Str(road, input[0].Key.c_str()) == input[0].Value &&
              field.Str(building, "kind") == "house",
          "all interned references resolve current strings");
    CHECK(field.Num(road, "width", -1) == 6.5 && field.Num(road, "height", -1) == 2 &&
              field.Num(road, "bridge", -1) == 1 && field.Num(road, "layer", -1) == 1,
          "road width, height, bridge and level survive pool reset");
    CHECK(field.Num(building, "height", -1) == 12 && field.Num(building, "tunnel", -1) == 1 &&
              field.Num(building, "layer", 0) == -1,
          "area height, tunnel and negative level survive");
    CHECK(road.MinLat == 1 && road.MaxLat == 3 && road.MinLon == 2 && road.MaxLon == 4 &&
              road.Type == 2 && building.Type == 3 && field.Rings()[1].Exterior,
          "geometry types, bounds and exterior ring remain correct");
    CHECK(std::ranges::equal(field.Points().first(4), input[0].LatLon) &&
              std::ranges::equal(field.Points().subspan(4), input[1].LatLon),
          "point data has no stale prefix");
  }
  field.Declare({}, Ground::TileAt{.X = 8, .Y = 9});
  CHECK(field.Features().empty() && field.Rings().empty() && field.Points().empty() &&
            field.KeyCount() == 0,
        "empty replacement removes all live content and keys");
  return Report();
}
