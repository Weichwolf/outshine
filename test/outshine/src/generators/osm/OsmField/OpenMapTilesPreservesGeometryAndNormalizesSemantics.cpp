#include "OsmField.h"
#include "MvtBuilding.h"
#include "Check.h"
#include "test/outshine/src/generators/osm/MvtLayer/WireFixture.h"
#include <array>
#include <span>
#include <string>
#include <string_view>

namespace {
using outshine::Test::Mvt::Append;
using outshine::Test::Mvt::Bytes;

Bytes String(std::string_view text) {
  Bytes value;
  Append(value, 0x0a, std::span(reinterpret_cast<const uint8_t *>(text.data()), text.size()));
  return value;
}

Bytes Layer(std::string_view name,
            int type,
            std::span<const std::string_view> keys,
            std::span<const Bytes> values) {
  Bytes layer;
  Append(layer, 0x0a, std::span(reinterpret_cast<const uint8_t *>(name.data()), name.size()));
  layer.insert(layer.end(), {0x78, 2, 0x28, 64});
  Bytes feature{0x08, 7, 0x18, static_cast<uint8_t>(type)};
  const Bytes polygon{9, 0, 0, 26, 20, 0, 0, 20, 19, 0, 15};
  const Bytes line{9, 0, 0, 10, 20, 20};
  Append(feature, 0x22, type == 3 ? polygon : line);
  Bytes tags;
  for (uint8_t index = 0; index < keys.size(); ++index) {
    tags.push_back(index);
    tags.push_back(index);
    Append(layer,
           0x1a,
           std::span(reinterpret_cast<const uint8_t *>(keys[index].data()), keys[index].size()));
    Append(layer, 0x22, values[index]);
  }
  Append(feature, 0x12, tags);
  Append(layer, 0x12, feature);
  Bytes tile;
  Append(tile, 0x1a, layer);
  return tile;
}
}

int main() {
  using namespace outshine::Generators::Osm;
  using namespace outshine::Test;
  Bytes tile =
      Layer("building",
            3,
            std::array<std::string_view, 3>{"render_height", "render_min_height", "material"},
            std::array{Bytes{0x28, 37}, Bytes{0x28, 4}, String("brick")});
  const Bytes streets = Layer("transportation",
                              2,
                              std::array<std::string_view, 3>{"class", "brunnel", "layer"},
                              std::array{String("minor"), String("bridge"), Bytes{0x28, 2}});
  const Bytes water =
      Layer("water", 3, std::array<std::string_view, 1>{"class"}, std::array{String("river")});
  tile.insert(tile.end(), streets.begin(), streets.end());
  tile.insert(tile.end(), water.begin(), water.end());
  const std::array<std::string, 4> names{
      "buildings", "streets", "water_polygons", "street_polygons"};
  OsmField field(2, names, MvtSchema::OpenMapTiles);
  CHECK(field.Accept(1, 1, tile) && field.Features().size() == 3,
        "explicit OpenMapTiles adapter publishes all three native layers");
  if (field.Features().size() != 3) { return Report(); }
  const auto &building = field.Features()[0];
  CHECK(field.Num(building, "height", -1) == 37 && field.Num(building, "min_height", -1) == 4 &&
            field.Str(building, "material") == "brick" && building.RingCount == 1 &&
            field.Rings()[building.FirstRing].Exterior &&
            field.Rings()[building.FirstRing].Count == 4 && building.ProviderFeatureId == 7,
        "independent footprint, clearance, material and feature identity survive normalization");
  const auto &road = field.Features()[1];
  CHECK(field.Str(road, "kind") == "residential" && field.Num(road, "bridge", 0) == 1 &&
            field.Num(road, "layer", 0) == 2 && road.Type == 2,
        "road class, bridge flag and independent physical layer are retained");
  CHECK(field.Str(field.Features()[2], "kind") == "river" && field.Features()[2].Type == 3,
        "river remains a separate polygon source");
  const auto snapshot = field.SnapshotQueries();
  CHECK(snapshot->Num(snapshot->Features()[0], "height", -1) == 37 &&
            snapshot->Str(snapshot->Features()[1], "kind") == "residential",
        "published snapshot retains normalized semantics");
  OsmField other(2, names);
  CHECK(other.Accept(1, 1, tile) && other.Features().empty(),
        "another schema is not silently guessed from names");
  CHECK(!ParseMvtSchema("unknown"), "unsupported schemas are explicit errors");
  const auto approximate = ReadMvtBuilding(field, building);
  CHECK(approximate.Height && approximate.Height->TopM == 37 &&
            approximate.Height->TopOrigin == outshine::Ground::BuildingHeightOrigin::Generated &&
            !approximate.Height->ConflictingLevels,
        "provider rendering estimates are not claimed as declared OSM heights");
  const Bytes contradictory =
      Layer("building",
            3,
            std::array<std::string_view, 2>{"render_height", "render_min_height"},
            std::array{Bytes{0x28, 5}, Bytes{0x28, 7}});
  OsmField ambiguous(2, names, MvtSchema::OpenMapTiles);
  CHECK(ambiguous.Accept(1, 1, contradictory), "contradictory source remains readable");
  const auto inferred = ReadMvtBuilding(ambiguous, ambiguous.Features()[0]);
  CHECK(inferred.Height && inferred.Height->TopM == 12 && inferred.Height->MinimumM == 7 &&
            inferred.Height->ConflictingLevels &&
            ambiguous.Num(ambiguous.Features()[0], "height", 0) == 5,
        "explicit inferred body retains clearance, conflicting source and footprint");
  const Bytes basement =
      Layer("building",
            3,
            std::array<std::string_view, 2>{"render_height", "render_min_height"},
            std::array{Bytes{0x28, 4}, Bytes{0x30, 21}});
  OsmField signedHeight(2, names, MvtSchema::OpenMapTiles);
  CHECK(signedHeight.Accept(1, 1, basement), "signed source height remains readable");
  const auto underground = ReadMvtBuilding(signedHeight, signedHeight.Features()[0]);
  CHECK(underground.Height && underground.Height->TopM == 4 &&
            underground.Height->MinimumM == -11 && !underground.Height->ConflictingLevels &&
            underground.Height->MinimumOrigin ==
                outshine::Ground::BuildingHeightOrigin::Generated &&
            signedHeight.Num(signedHeight.Features()[0], "min_height", 0) == -11,
        "independent signed protobuf fixture retains basement and canonical estimated endpoint");
  const Bytes outline =
      Layer("building", 3, std::array<std::string_view, 1>{"hide_3d"}, std::array{Bytes{0x38, 1}});
  OsmField outlines(2, names, MvtSchema::OpenMapTiles);
  CHECK(outlines.Accept(1, 1, outline) && outlines.Features().size() == 1 &&
            ReadMvtBuilding(outlines, outlines.Features()[0]).Hidden,
        "outline remains available as source geometry but does not duplicate 3D parts");
  const auto absent = ReadMvtBuilding(outlines, outlines.Features()[0]);
  CHECK(absent.Height && !absent.Height->ConflictingLevels && absent.Height->TopM == 5,
        "absent coarse height is a generated estimate, not a contradictory source");
  return Report();
}
