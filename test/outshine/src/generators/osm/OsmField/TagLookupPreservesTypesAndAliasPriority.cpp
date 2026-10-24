#include "Check.h"
#include "MvtLayer.h"
#include "MvtSchema.h"
#include "MvtWireFixture.h"
#include "OsmField.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace {
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
using outshine::Test::Mvt::Append;
using outshine::Test::Mvt::Bytes;
enum class Kind : uint8_t { Any, Number, String };
constexpr uint8_t kLayerVersion = 0x78;
constexpr uint8_t kLayerExtent = 0x28;
constexpr uint8_t kFeatureType = 0x18;
constexpr uint8_t kFeatureGeometry = 0x22;
constexpr uint8_t kLayerKey = 0x1a;
constexpr uint8_t kLayerValue = 0x22;
constexpr uint8_t kFeatureTags = 0x12;
constexpr uint8_t kLayerFeature = 0x12;
constexpr uint8_t kTileLayer = 0x1a;
constexpr uint8_t kUnsignedValue = 0x28;
constexpr uint8_t kLineOffset = 20;

struct InputTag {
  std::string_view Key;
  Bytes Value;
};

Bytes Text(std::string_view text) {
  Bytes result;
  Append(result, 0x0a, {reinterpret_cast<const uint8_t *>(text.data()), text.size()});
  return result;
}

Bytes Tile(std::string_view name, std::span<const InputTag> tags) {
  Bytes layer;
  Append(layer, 0x0a, {reinterpret_cast<const uint8_t *>(name.data()), name.size()});
  layer.insert(layer.end(), {kLayerVersion, 2, kLayerExtent, 64});
  Bytes feature{kFeatureType, 2, kFeatureGeometry, 6, 9, 0, 0, 10, kLineOffset, kLineOffset};
  Bytes pairs;
  for (size_t at = 0; at < tags.size(); ++at) {
    const auto &tag = tags[at];
    Append(layer, kLayerKey, {reinterpret_cast<const uint8_t *>(tag.Key.data()), tag.Key.size()});
    Append(layer, kLayerValue, tag.Value);
    pairs.push_back(static_cast<uint8_t>(at));
    pairs.push_back(static_cast<uint8_t>(at));
  }
  Append(feature, kFeatureTags, pairs);
  Append(layer, kLayerFeature, feature);
  Bytes result;
  Append(result, kTileLayer, layer);
  return result;
}

void Query(std::initializer_list<InputTag> tags,
           std::string_view key,
           Kind kind,
           std::optional<MvtLayer::Tag> expected,
           std::string_view layer = "streets",
           MvtSchema schema = MvtSchema::OpenMapTiles) {
  const std::array<std::string, 1> layers{std::string(layer)};
  OsmField field(2, layers, schema);
  CHECK(field.Accept(1, 1, Tile(MvtLayerName(schema, layer), {tags.begin(), tags.size()})),
        "the original ordered and repeated MVT tags are accepted");
  if (field.Features().empty()) {
    CHECK(false, "the source feature is retained");
    return;
  }
  const auto &feature = field.Features().front();
  const std::string name(key);
  if (kind == Kind::String) {
    CHECK(field.Str(feature, name.c_str()) == (expected ? expected->String : std::string_view{}),
          "string lookup keeps type rejection and explicit/alias precedence");
  } else if (kind == Kind::Number) {
    const double got = field.Num(feature, name.c_str(), std::numeric_limits<double>::quiet_NaN());
    CHECK(expected ? got == expected->Number : std::isnan(got),
          "number lookup keeps type rejection and explicit/alias precedence");
  } else {
    CHECK(field.Has(feature, key) == expected.has_value(), "presence keeps untyped alias rules");
    if (expected) {
      const auto integer = field.Integer(feature, key);
      CHECK(expected->IsNumber ? integer && *integer && **integer == expected->Number : !integer,
            "integer lookup uses the first untyped source or alias, not a later typed match");
    }
  }
}

MvtLayer::Tag String(std::string_view key, std::string_view text) {
  return {.Key = key, .String = text, .Number = 0, .IsNumber = false};
}

MvtLayer::Tag Number(std::string_view key, double number) {
  return {.Key = key, .String = {}, .Number = number, .IsNumber = true};
}

void ExplicitTypes() {
  const InputTag wrong{.Key = "kind", .Value = {kUnsignedValue, 7}};
  const InputTag alias{.Key = "class", .Value = Text("path")};
  Query({alias, wrong}, "kind", Kind::String, std::nullopt);
  Query({wrong, alias}, "kind", Kind::String, std::nullopt);
  Query({alias, wrong}, "kind", Kind::Any, Number("kind", 7));
  Query({wrong, alias}, "kind", Kind::Number, Number("kind", 7));
  Query({{.Key = "class", .Value = {kUnsignedValue, 7}}, {.Key = "kind", .Value = Text("")}},
        "kind",
        Kind::Number,
        std::nullopt);
  Query({{.Key = "class", .Value = {kUnsignedValue, 7}}, {.Key = "kind", .Value = Text("")}},
        "kind",
        Kind::Any,
        String("kind", ""));
  Query({wrong, {.Key = "kind", .Value = Text("road")}, alias},
        "kind",
        Kind::String,
        String("kind", "road"));
  Query(
      {{.Key = "class", .Value = {kUnsignedValue, 7}}, alias}, "kind", Kind::String, std::nullopt);
  Query({{.Key = "class", .Value = {kUnsignedValue, 7}}, alias},
        "kind",
        Kind::Any,
        Number("kind", 7));
  Query({alias, {.Key = "class", .Value = {kUnsignedValue, 7}}},
        "kind",
        Kind::String,
        String("kind", "path"));
}

void SubclassesAndBrunnels() {
  Query({{.Key = "brunnel", .Value = Text("bridge")},
         {.Key = "bridge", .Value = {kUnsignedValue, 0}}},
        "bridge",
        Kind::Number,
        Number("bridge", 0));
  Query({{.Key = "bridge", .Value = {kUnsignedValue, 0}},
         {.Key = "brunnel", .Value = Text("bridge")}},
        "bridge",
        Kind::Any,
        Number("bridge", 0));
  Query({{.Key = "class", .Value = Text("path")},
         {.Key = "subclass", .Value = {kUnsignedValue, 7}},
         {.Key = "subclass", .Value = Text("steps")}},
        "kind",
        Kind::String,
        String("kind", "steps"));
  Query({{.Key = "class", .Value = Text("transit")}, {.Key = "subclass", .Value = Text("rail")}},
        "kind",
        Kind::String,
        String("kind", "rail"));
  Query({{.Key = "class", .Value = Text("minor")}, {.Key = "subclass", .Value = Text("steps")}},
        "kind",
        Kind::String,
        String("kind", "residential"));
  Query({{.Key = "brunnel", .Value = Text("bridge")}, {.Key = "bridge", .Value = Text("no")}},
        "bridge",
        Kind::Number,
        std::nullopt);
  Query({{.Key = "bridge", .Value = Text("no")}, {.Key = "brunnel", .Value = Text("bridge")}},
        "bridge",
        Kind::Any,
        String("bridge", "no"));
  Query({{.Key = "brunnel", .Value = Text("bridge")}, {.Key = "brunnel", .Value = Text("tunnel")}},
        "tunnel",
        Kind::Number,
        Number("tunnel", 1));
  Query({{.Key = "brunnel", .Value = Text("other")}, {.Key = "brunnel", .Value = Text("bridge")}},
        "bridge",
        Kind::Number,
        Number("bridge", 1));
  Query({{.Key = "brunnel", .Value = {kUnsignedValue, 7}}}, "bridge", Kind::Any, std::nullopt);
}

void BuildingsAndShortbread() {
  for (const bool aliasFirst : {false, true}) {
    const InputTag wrong{.Key = "height", .Value = Text("bad")};
    const InputTag alias{.Key = "render_height", .Value = {kUnsignedValue, 7}};
    Query(aliasFirst ? std::initializer_list<InputTag>{alias, wrong}
                     : std::initializer_list<InputTag>{wrong, alias},
          "height",
          Kind::Number,
          std::nullopt,
          "buildings");
  }
  Query({{.Key = "render_height", .Value = {kUnsignedValue, 7}}},
        "height",
        Kind::Number,
        Number("height", 7),
        "buildings");
  Query({{.Key = "render_height", .Value = {kUnsignedValue, 7}}},
        "render_height",
        Kind::Number,
        Number("render_height", 7),
        "buildings");
  Query({{.Key = "render_min_height", .Value = {kUnsignedValue, 2}}},
        "min_height",
        Kind::Number,
        Number("min_height", 2),
        "buildings");
  Query({{.Key = "colour", .Value = Text("white")},
         {.Key = "building:colour", .Value = {kUnsignedValue, 7}}},
        "building:colour",
        Kind::String,
        std::nullopt,
        "buildings");
  Query({{.Key = "colour", .Value = Text("white")}},
        "building:colour",
        Kind::String,
        String("building:colour", "white"),
        "buildings");
  Query({{.Key = "class", .Value = Text("minor")}},
        "kind",
        Kind::String,
        std::nullopt,
        "streets",
        MvtSchema::Shortbread);
  Query({{.Key = "class", .Value = Text("minor")}},
        "class",
        Kind::String,
        String("class", "minor"),
        "streets",
        MvtSchema::Shortbread);
  Query({{.Key = "brunnel", .Value = Text("bridge")}}, "bridge", Kind::Any, std::nullopt, "land");
  Query({{.Key = "render_height", .Value = {kUnsignedValue, 7}}},
        "height",
        Kind::Any,
        std::nullopt,
        "land");
  const std::array<std::string, 0> none{};
  const OsmField empty(2, none);
  CHECK(!empty.Has({}, "missing"), "an empty feature needs neither tags nor a layer");
}
}

int main() {
  ExplicitTypes();
  SubclassesAndBrunnels();
  BuildingsAndShortbread();
  return Report();
}
