#include "OsmField.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators::Osm;
  using namespace outshine::Test;
  const std::vector<OsmField::Declared> original{
      {.Layer = "transportation", .Key = "kind", .Value = "road", .LatLon = {1, 2, 3, 4}},
      {.Layer = "building", .Key = "kind", .Value = "house", .Area = true, .LatLon = {5, 6, 7, 8}}};
  const auto identity = OsmField::DeclarationDigest(original);
  CHECK(identity && !identity->empty() && identity == OsmField::DeclarationDigest(original),
        "the full declaration has a reproducible owned identity");
  const auto empty = OsmField::DeclarationDigest({});
  CHECK(empty && !empty->empty() && empty != identity,
        "an explicitly empty input has its own stable persistent identity");
  constexpr int mutations = 14;
  for (int change = 0; change < mutations; ++change) {
    auto input = original;
    auto &feature = input.front();
    switch (change) {
      case 0: feature.Layer = "building"; break;
      case 1: feature.Key = "class"; break;
      case 2: feature.Value = "path"; break;
      case 3: feature.WidthM = 1; break;
      case 4: feature.HeightM = 1; break;
      case 5: feature.Area = true; break;
      case 6: feature.Bridge = true; break;
      case 7: feature.Tunnel = true; break;
      case 8: feature.Level = -1; break;
      case 9: feature.LatLon[0] = std::nextafter(feature.LatLon[0], 2.0); break;
      case 10: feature.LatLon.push_back(1); break;
      case 11: input.push_back(input.back()); break;
      case 12: input.pop_back(); break;
      default: std::swap(input[0], input[1]); break;
    }
    CHECK(OsmField::DeclarationDigest(input) != identity,
          "every field, point bit, input count and input order participates in identity");
  }
  const std::array<std::string, 2> layers{"transportation", "building"};
  OsmField field(2, layers);
  const Ground::TileAt tile{.X = 1, .Y = 1};
  field.Declare(original, tile, *identity);
  CHECK(field.Tiles().front().InputDigest == *identity, "bound inputs retain their true identity");
  const auto generation = field.Generation();
  field.Declare(original, tile, *identity);
  CHECK(field.Generation() == generation, "unchanged inputs avoid reconstruction");
  field.Declare(original, tile);
  CHECK(field.Tiles().front().InputDigest.empty() && field.Generation() > generation,
        "an explicitly unbound declaration cannot inherit a previous persistent binding");
  return Report();
}
