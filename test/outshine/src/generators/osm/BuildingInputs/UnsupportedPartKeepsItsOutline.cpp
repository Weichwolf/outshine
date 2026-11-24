#include "BuildingInputs.h"
#include "Check.h"
#include "MvtWireFixture.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace {

using outshine::Test::Mvt::Append;
using outshine::Test::Mvt::Bytes;

void AddRectangle(
    Bytes &layer, uint8_t origin, uint8_t side, uint8_t height, uint8_t minimum, bool hidden) {
  Bytes feature{0x18, 3};
  Bytes tags{0, height, 1, minimum};
  if (hidden) { tags.insert(tags.end(), {2, 1}); }
  Append(feature, 0x12, tags);
  Append(feature,
         0x22,
         Bytes{9, origin, origin, 26, side, 0, 0, side, static_cast<uint8_t>(side - 1), 0, 15});
  Append(layer, 0x12, feature);
}

Bytes Tile(bool hasSupport, uint8_t partOrigin) {
  Bytes layer{0x0a, 8, 'b', 'u', 'i', 'l', 'd', 'i', 'n', 'g', 0x78, 2, 0x28, 64};
  for (const std::string_view key : {"render_height", "render_min_height", "hide_3d"}) {
    Append(layer, 0x1a, {reinterpret_cast<const uint8_t *>(key.data()), key.size()});
  }
  for (const uint8_t value : {uint8_t{0}, uint8_t{1}, uint8_t{15}, uint8_t{40}}) {
    Append(layer, 0x22, Bytes{0x28, value});
  }
  AddRectangle(layer, 4, 24, 3, 0, true);
  AddRectangle(layer, partOrigin, 4, 3, 2, false);
  if (hasSupport) { AddRectangle(layer, partOrigin, 4, 2, 0, false); }
  Bytes tile{0x1a};
  const auto size = static_cast<uint8_t>(layer.size());
  if (size >= 128) { tile.push_back(static_cast<uint8_t>(size | 128)); }
  tile.push_back(size >= 128 ? 1 : size);
  tile.insert(tile.end(), layer.begin(), layer.end());
  return tile;
}

void CheckTile(bool hasSupport, uint8_t partOrigin, bool restoresOutline) {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Generators::Osm;
  using namespace outshine::Test;
  const std::array<std::string, 1> layers{"buildings"};
  OsmField field(14, layers, MvtSchema::OpenMapTiles);
  CHECK(field.Accept(1, 1, Tile(hasSupport, partOrigin)), "vector source is accepted");
  CHECK(field.Tiles().size() == 1, "the tile is available to building preparation");
  if (field.Tiles().empty()) { return; }
  RawTile raw;
  PrepareBuildingInputs(field,
                        BuildingField{},
                        StreetField{},
                        {.From = 0, .To = field.Features().size(), .Tile = 0, .Found = true},
                        {},
                        std::nullopt,
                        std::nullopt,
                        raw);
  const auto expected = 1u + (hasSupport || restoresOutline ? 1u : 0u);
  CHECK(raw.Structures.size() == expected, "only an unsupported part needs its hidden outline");
  CHECK(std::ranges::any_of(
            raw.Structures,
            [](const auto &shape) { return shape.MinimumHeightM == 15 && shape.HeightM == 40; }),
        "the elevated part retains the source heights");
  if (restoresOutline && raw.Structures.size() == expected) {
    CHECK(std::ranges::any_of(raw.Structures,
                              [](const auto &shape) {
                                return shape.MinimumHeightM == 0 && shape.HeightM == 15 &&
                                       shape.Pitched == 0;
                              }),
          "the outline fills only the unrepresented base");
  }
}

}

int main() {
  CheckTile(false, 8, true);
  CheckTile(true, 8, false);
  CheckTile(false, 32, false);
  return outshine::Test::Report();
}
