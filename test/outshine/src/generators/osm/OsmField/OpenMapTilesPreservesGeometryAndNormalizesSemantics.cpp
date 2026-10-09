#include "OsmField.h"
#include "BuildingProperties.h"
#include "GroundMaterials.h"
#include "StreetField.h"
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

void CheckBuildingColours() {
  using namespace outshine::Generators::Osm;
  using namespace outshine::Test;
  const std::array<std::string, 1> layers{"buildings"};
  for (const bool sourceFirst : {false, true}) {
    const auto keys = sourceFirst ? std::array<std::string_view, 2>{"building:colour", "colour"}
                                  : std::array<std::string_view, 2>{"colour", "building:colour"};
    const auto values = sourceFirst ? std::array{String("#808080"), String("white")}
                                    : std::array{String("white"), String("#808080")};
    OsmField field(2, layers, MvtSchema::OpenMapTiles);
    CHECK(field.Accept(1, 1, Layer("building", 3, keys, values)), "source colours are accepted");
    if (field.Features().empty()) { continue; }
    const auto &feature = field.Features()[0];
    const auto building = ReadBuildingProperties(field, feature);
    CHECK(building.WallColour && !building.WallColourRejected &&
              field.Str(feature, "colour") == "white" &&
              field.Str(feature, "building:colour") == "#808080",
          "canonical source paint wins in either tag order without replacing raw tags");
    if (building.WallColour) {
      for (const float channel : *building.WallColour) {
        CHECK_NEAR(channel, 0.2158605, 0.000001, "linear gray", "source sRGB is linearized");
      }
    }
  }
  for (const auto &source :
       {String("transparent"), String("unknown"), String("xxxxxx"), Bytes{0x28, 99}}) {
    OsmField field(2, layers, MvtSchema::OpenMapTiles);
    CHECK(field.Accept(1,
                       1,
                       Layer("building",
                             3,
                             std::array<std::string_view, 2>{"colour", "building:colour"},
                             std::array{String("white"), source})),
          "invalid supplied colours retain their source record");
    if (field.Features().empty()) { continue; }
    const auto building = ReadBuildingProperties(field, field.Features()[0]);
    CHECK(!building.WallColour && building.WallColourRejected,
          "invalid canonical paint cannot silently fall back to an opaque alias");
  }
  OsmField alias(2, layers, MvtSchema::OpenMapTiles);
  CHECK(alias.Accept(1,
                     1,
                     Layer("building",
                           3,
                           std::array<std::string_view, 1>{"colour"},
                           std::array{String("white")})),
        "provider colour alias is accepted");
  if (!alias.Features().empty()) {
    const auto building = ReadBuildingProperties(alias, alias.Features()[0]);
    CHECK(building.WallColour && !building.WallColourRejected &&
              (*building.WallColour)[0] == 1.0f && (*building.WallColour)[1] == 1.0f &&
              (*building.WallColour)[2] == 1.0f,
          "available provider paint reaches the building plan");
  }
}

void CheckUnprefixedBuildingColours() {
  using namespace outshine::Generators::Osm;
  using namespace outshine::Test;
  const std::array<std::string, 1> layers{"buildings"};
  constexpr std::array expected{0.4286904966, 0.3419144249, 0.2232279573};
  for (const std::string_view key : {"colour", "building:colour"}) {
    for (const std::string_view supplied : {"af9e82", "AF9E82", "#af9e82"}) {
      OsmField field(2, layers, MvtSchema::OpenMapTiles);
      CHECK(field.Accept(1, 1, Layer("building", 3, std::array{key}, std::array{String(supplied)})),
            "observed provider RGB encodings enter the normal vector tile path");
      if (field.Features().empty()) { continue; }
      const auto &feature = field.Features().front();
      const auto building = ReadBuildingProperties(field, feature);
      CHECK(building.WallColour && !building.WallColourRejected &&
                field.Str(feature, key.data()) == supplied,
            "six-digit provider RGB is used without rewriting supplied tags");
      if (!building.WallColour) { continue; }
      for (size_t channel = 0; channel < expected.size(); ++channel) {
        CHECK_NEAR((*building.WallColour)[channel],
                   expected[channel],
                   1e-6,
                   "linear RGB",
                   "equivalent provider encodings preserve channel order and source paint");
      }
    }
  }
}

void CheckExplicitValuePrecedence() {
  using namespace outshine::Generators::Osm;
  using namespace outshine::Test;
  const std::array<std::string, 1> layers{"buildings"};
  for (const bool sourceFirst : {false, true}) {
    const auto keys = sourceFirst ? std::array<std::string_view, 2>{"height", "render_height"}
                                  : std::array<std::string_view, 2>{"render_height", "height"};
    const auto values = sourceFirst ? std::array{Bytes{0x28, 19}, Bytes{0x28, 37}}
                                    : std::array{Bytes{0x28, 37}, Bytes{0x28, 19}};
    OsmField field(2, layers, MvtSchema::OpenMapTiles);
    CHECK(field.Accept(1, 1, Layer("building", 3, keys, values)),
          "independent source and derived heights are accepted in either tag order");
    if (field.Features().empty()) { continue; }
    const auto &feature = field.Features()[0];
    const auto integer = field.Integer(feature, "height");
    CHECK(field.Num(feature, "height", -1) == 19 && integer && *integer == 19 &&
              field.Num(feature, "render_height", -1) == 37 && feature.TagCount == 4,
          "explicit height wins without duplicating tags or losing derived source data");
  }
  OsmField invalid(2, layers, MvtSchema::OpenMapTiles);
  CHECK(invalid.Accept(1,
                       1,
                       Layer("building",
                             3,
                             std::array<std::string_view, 2>{"render_height", "height"},
                             std::array{Bytes{0x28, 37}, String("unknown")})),
        "non-numeric explicit source value remains representable");
  if (!invalid.Features().empty()) {
    const auto &feature = invalid.Features()[0];
    CHECK(invalid.Str(feature, "height") == "unknown" && invalid.Num(feature, "height", -1) == -1 &&
              !invalid.Integer(feature, "height"),
          "invalid explicit height remains visible instead of falling back to an alias");
  }
}

void CheckRailClassRules() {
  using namespace outshine::Generators::Osm;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  GroundMaterials materials;
  VegetationTemplates templates;
  CHECK(materials.Load("src/assets/world/ground-materials.json"), "ground catalog loads");
  CHECK(templates.Load("src/assets/world/vegetation.json", materials), "street rules load");
  if (!templates.Ready()) { return; }
  const std::array<std::string, 1> layers{"streets"};
  OsmField field(2, layers, MvtSchema::OpenMapTiles);
  CHECK(field.Accept(1,
                     1,
                     Layer("transportation",
                           2,
                           std::array<std::string_view, 3>{"class", "brunnel", "layer"},
                           std::array{String("rail"), String("bridge"), Bytes{0x28, 2}})),
        "provider railway geometry is accepted");
  if (field.Features().empty()) { return; }
  const auto &feature = field.Features().front();
  CHECK(field.Str(feature, "class") == "rail" && field.Str(feature, "kind") == "rail",
        "railway class reaches the canonical street recipe without losing the source tag");
  StreetField streets;
  CHECK(streets.Ingest(field, templates) == 1 && streets.UnruledCount() == 0,
        "delivered railway becomes a native corridor instead of being silently discarded");
  if (streets.Ways().empty()) { return; }
  const auto &way = streets.Ways().front();
  CHECK(way.Bridge && way.Layer == 2 && way.PointCount == 2 && way.CoverRow >= 0,
        "rail bridge retains its independent level, geometry and surface material");
  CHECK_NEAR(way.HalfWidthM, 1.9, 1e-6, "half width", "railway recipe controls corridor width");
}

void CheckTransitClassRules() {
  using namespace outshine::Generators::Osm;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  GroundMaterials materials;
  VegetationTemplates templates;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            templates.Load("src/assets/world/vegetation.json", materials),
        "production transit recipes load");
  if (!templates.Ready()) { return; }
  const std::array<std::string, 1> layers{"streets"};
  for (const std::string_view subclass : {"tram", "subway", "light_rail"}) {
    for (const bool reverse : {false, true}) {
      const auto keys =
          reverse ? std::array<std::string_view, 4>{"subclass", "class", "brunnel", "layer"}
                  : std::array<std::string_view, 4>{"class", "subclass", "brunnel", "layer"};
      const auto values =
          reverse
              ? std::array{String(subclass), String("transit"), String("bridge"), Bytes{0x28, 2}}
              : std::array{String("transit"), String(subclass), String("bridge"), Bytes{0x28, 2}};
      OsmField field(2, layers, MvtSchema::OpenMapTiles);
      CHECK(field.Accept(1, 1, Layer("transportation", 2, keys, values)),
            "transit source is accepted");
      if (field.Features().empty()) { continue; }
      const auto &feature = field.Features().front();
      CHECK(field.Str(feature, "class") == "transit" &&
                field.Str(feature, "subclass") == subclass &&
                field.Str(feature, "kind") == subclass,
            "canonical transit class follows the supplied subtype in either source tag order");
      StreetField streets;
      CHECK(streets.Ingest(field, templates) == 1 && streets.UnruledCount() == 0 &&
                streets.Ways().front().Bridge && streets.Ways().front().Layer == 2,
            "aboveground transit reaches its native recipe and preserves its bridge level");
      const auto snapshot = field.SnapshotQueries();
      CHECK(snapshot->Str(snapshot->Features().front(), "kind") == subclass,
            "published source snapshot keeps the same transit semantics");
    }
  }
  for (const auto &subclass : {String(""), Bytes{0x28, 9}}) {
    OsmField field(2, layers, MvtSchema::OpenMapTiles);
    CHECK(field.Accept(1,
                       1,
                       Layer("transportation",
                             2,
                             std::array<std::string_view, 2>{"class", "subclass"},
                             std::array{String("transit"), subclass})),
          "incomplete transit source remains readable");
    if (!field.Features().empty()) {
      CHECK(field.Str(field.Features().front(), "kind") == "transit",
            "missing subtype is not guessed");
    }
  }
  OsmField explicitKind(2, layers, MvtSchema::OpenMapTiles);
  CHECK(explicitKind.Accept(1,
                            1,
                            Layer("transportation",
                                  2,
                                  std::array<std::string_view, 3>{"class", "subclass", "kind"},
                                  std::array{String("transit"), String("tram"), String("rail")})),
        "explicit canonical class is accepted");
  if (!explicitKind.Features().empty()) {
    CHECK(explicitKind.Str(explicitKind.Features().front(), "kind") == "rail",
          "supplied canonical class wins over transit aliases");
  }
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
  CHECK(field.Num(building, "render_height", -1) == 37 &&
            field.Num(building, "render_min_height", -1) == 4 && building.TagCount == 6,
        "original building attributes remain readable without alias copies");
  const auto &road = field.Features()[1];
  CHECK(field.Str(road, "kind") == "residential" && field.Num(road, "bridge", 0) == 1 &&
            field.Num(road, "layer", 0) == 2 && road.Type == 2,
        "road class, bridge flag and independent physical layer are retained");
  const auto bridge = field.Integer(road, "bridge");
  CHECK(field.Str(road, "class") == "minor" && field.Str(road, "brunnel") == "bridge" &&
            road.TagCount == 6 && bridge && *bridge == 1,
        "raw road class and brunnel survive string-to-string and string-to-number aliases");
  CHECK(field.Str(field.Features()[2], "kind") == "river" && field.Features()[2].Type == 3,
        "river remains a separate polygon source");
  const auto snapshot = field.SnapshotQueries();
  CHECK(snapshot->Num(snapshot->Features()[0], "height", -1) == 37 &&
            snapshot->Num(snapshot->Features()[0], "render_height", -1) == 37 &&
            snapshot->Str(snapshot->Features()[1], "kind") == "residential" &&
            snapshot->Str(snapshot->Features()[1], "class") == "minor",
        "published snapshot retains both original and canonical semantics");
  OsmField other(2, names);
  CHECK(other.Accept(1, 1, tile) && other.Features().empty(),
        "another schema is not silently guessed from names");
  CHECK(!ParseMvtSchema("unknown"), "unsupported schemas are explicit errors");
  const auto approximate = ReadBuildingProperties(field, building);
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
  const auto inferred = ReadBuildingProperties(ambiguous, ambiguous.Features()[0]);
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
  const auto underground = ReadBuildingProperties(signedHeight, signedHeight.Features()[0]);
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
            ReadBuildingProperties(outlines, outlines.Features()[0]).Hidden,
        "outline remains available as source geometry but does not duplicate 3D parts");
  const auto absent = ReadBuildingProperties(outlines, outlines.Features()[0]);
  CHECK(absent.Height && !absent.Height->ConflictingLevels && absent.Height->TopM == 5,
        "absent coarse height is a generated estimate, not a contradictory source");
  CheckExplicitValuePrecedence();
  CheckBuildingColours();
  CheckUnprefixedBuildingColours();
  CheckRailClassRules();
  CheckTransitClassRules();
  return Report();
}
