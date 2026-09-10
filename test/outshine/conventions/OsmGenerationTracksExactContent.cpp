#include "OsmField.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <cfenv>
#include <limits>
#include <string>
#include <utility>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array<std::string, 2> layers{"transportation", "building"};
  Ground::OsmField field(14, layers);
  const std::vector<Ground::OsmField::Declared> original{{.Layer = "transportation",
                                                          .Key = "kind",
                                                          .Value = "road",
                                                          .WidthM = 3,
                                                          .HeightM = 4,
                                                          .LatLon = {1, 2, 3, 4, 5, 6}},
                                                         {.Layer = "building",
                                                          .Key = "kind",
                                                          .Value = "house",
                                                          .Area = true,
                                                          .LatLon = {7, 8, 9, 10, 7, 10}}};
  const Ground::TileAt tile{.X = 8, .Y = 9};
  for (int change = 0; change < 22; ++change) {
    field.Declare(original, tile);
    const auto before = field.Generation();
    auto input = original;
    auto over = tile;
    auto &feature = input.front();
    switch (change) {
      case 0: feature.LatLon[0] = std::nextafter(feature.LatLon[0], 2.0); break;
      case 1: feature.LatLon[1] = std::nextafter(feature.LatLon[1], 3.0); break;
      case 2: feature.WidthM = std::nextafter(feature.WidthM, 4.0); break;
      case 3: feature.HeightM = std::nextafter(feature.HeightM, 5.0); break;
      case 4: feature.Value = "track"; break;
      case 5: feature.Key = "class"; break;
      case 6: feature.Layer = "building"; break;
      case 7: feature.Area = true; break;
      case 8: feature.Bridge = true; break;
      case 9: feature.Tunnel = true; break;
      case 10: feature.Level = -1; break;
      case 11: input.push_back(input.back()); break;
      case 12: input.pop_back(); break;
      case 13: std::swap(input[0], input[1]); break;
      case 14: ++over.X; break;
      case 15: input.clear(); break;
      case 16: ++over.Y; break;
      case 17: feature.WidthM = 0; break;
      case 18: feature.HeightM = 0; break;
      case 19:
        feature.Key = "width";
        feature.Value.clear();
        break;
      case 20: feature.WidthM = std::numeric_limits<double>::max(); break;
      default: feature.LatLon.resize(4); break;
    }
    std::feclearexcept(FE_ALL_EXCEPT);
    field.Declare(input, over);
    CHECK(std::fetestexcept(FE_INVALID) == 0,
          "finite content does not trigger invalid numeric conversion");
    const auto changed = field.Generation();
    CHECK(changed != before, "every effective content or tile change invalidates derived data");
    const auto bytes = field.HeapBytes();
    field.Declare(input, over);
    CHECK(field.Generation() == changed && field.HeapBytes() == bytes,
          "identical redeclaration preserves generation and storage");
  }
  return Report();
}
