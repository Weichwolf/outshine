#include "Check.h"
#include "GroundMaterialCatalog.h"
#include "MvtWireFixture.h"
#include "OsmSurfacePlacement.h"
#include "OsmFootpathSource.h"
#include "StreetField.h"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators::Osm;
using namespace outshine::Ground;
using namespace outshine::Test;
using namespace outshine::Test::Mvt;
constexpr uint8_t kLayerName = 0x0a;
constexpr uint8_t kLayerKey = 0x1a;
constexpr uint8_t kLayerValue = 0x22;
constexpr uint8_t kLayerFeature = 0x12;
constexpr uint8_t kLayerVersion = 0x78;
constexpr uint8_t kLayerExtent = 0x28;
constexpr uint8_t kTileLayer = 0x1a;
constexpr uint8_t kFeatureType = 0x18;
constexpr uint8_t kFeatureGeometry = 0x22;
constexpr uint8_t kFeatureTags = 0x12;
constexpr uint8_t kDoubleValue = 0x19;

Bytes Text(std::string_view value) {
  Bytes result{0x0a, static_cast<uint8_t>(value.size())};
  result.insert(result.end(), value.begin(), value.end());
  return result;
}

Bytes Number(double value) {
  Bytes result{kDoubleValue};
  auto bits = std::bit_cast<uint64_t>(value);
  for (unsigned at = 0; at < sizeof(double); ++at) {
    result.push_back(static_cast<uint8_t>(bits & std::numeric_limits<uint8_t>::max()));
    bits >>= 8;
  }
  return result;
}

struct Case {
  MvtSchema Schema = MvtSchema::Shortbread;
  std::string Kind = "path";
  std::optional<std::string> Subclass = std::nullopt;
  std::optional<Bytes> Foot = std::nullopt;
  std::optional<Bytes> Access = std::nullopt;
  std::optional<Bytes> Layer = std::nullopt;
  std::optional<Bytes> Bridge = std::nullopt;
  std::optional<Bytes> Tunnel = std::nullopt;
  std::optional<std::string> Brunnel = std::nullopt;
  SurfacePlacement Expected{};
  std::optional<SurfacePlacementError> Error = std::nullopt;
};

Bytes Tile(const Case &input) {
  Bytes layer;
  const auto *const name = input.Schema == MvtSchema::Shortbread ? "streets" : "transportation";
  const auto layerName = Text(name);
  Append(layer, kLayerName, std::span<const uint8_t>(layerName).subspan(2));
  Bytes tags;
  uint8_t next = 0;
  const auto add = [&](std::string_view key, const std::optional<Bytes> &value) {
    if (!value) { return; }
    const auto text = Text(key);
    Append(layer, kLayerKey, std::span<const uint8_t>(text).subspan(2));
    Append(layer, kLayerValue, *value);
    tags.insert(tags.end(), {next, next});
    ++next;
  };
  add(input.Schema == MvtSchema::Shortbread ? "kind" : "class", Text(input.Kind));
  if (input.Subclass) { add("subclass", Text(*input.Subclass)); }
  add("foot", input.Foot);
  add("access", input.Access);
  add("layer", input.Layer);
  add("bridge", input.Bridge);
  add("tunnel", input.Tunnel);
  if (input.Brunnel) { add("brunnel", Text(*input.Brunnel)); }
  Bytes feature{kFeatureType, 2, kFeatureGeometry, 6, 9, 0, 0, 10, 2, 2};
  Append(feature, kFeatureTags, tags);
  Append(layer, kLayerFeature, feature);
  layer.insert(layer.end(), {kLayerVersion, 2, kLayerExtent, 0x80, 0x20});
  Bytes result;
  Append(result, kTileLayer, layer);
  return result;
}

void CheckCase(const Case &input, const outshine::Generators::RegionSurfaceRecipes &templates) {
  const std::array<std::string, 1> layers{"streets"};
  OsmField field(1, layers, input.Schema);
  const auto accepted = field.Accept(0, 0, Tile(input));
  CHECK(accepted && field.Features().size() == 1, "real MVT bytes supply one source feature");
  if (!accepted || field.Features().size() != 1) { return; }
  const auto invalidIndex = ReadSurfacePlacement(field, 1);
  CHECK(!invalidIndex && invalidIndex.error() == SurfacePlacementError::InvalidFeature,
        "a stale or unbound source index cannot read unrelated feature tags");
  const auto placement = ReadSurfacePlacement(field, 0);
  CHECK(placement.has_value() == !input.Error.has_value(),
        "presence and malformed placement values remain distinct");
  StreetField streets;
  const auto count = streets.Ingest(field, templates);
  if (input.Error) {
    CHECK(!placement && placement.error() == *input.Error,
          "malformed or conflicting tags retain their specific failure");
    CHECK(count == 0 && streets.InvalidPlacementCount() == 1,
          "invalid placement is not silently published as ground infrastructure");
    return;
  }
  if (!placement) { return; }
  CHECK(placement->Layer == input.Expected.Layer && placement->Bridge == input.Expected.Bridge &&
            placement->Tunnel == input.Expected.Tunnel,
        "native source placement preserves supplied, absent and normalized facts");
  const bool tunnel = placement->Tunnel.value_or(false);
  CHECK(count == (tunnel ? 0u : 1u) && streets.TunnelCount() == (tunnel ? 1 : 0),
        "legacy display selection remains separate from the retained source facts");
  if (!tunnel && count == 1) {
    CHECK(streets.Ways()[0].Bridge == placement->Bridge.value_or(false) &&
              streets.Ways()[0].Layer == placement->Layer.value_or(0),
          "the live producer uses the same checked placement facts");
  }
  const auto after = ReadSurfacePlacement(field, 0);
  CHECK(after && after->Tunnel == placement->Tunnel && field.Features().size() == 1,
        "display filtering leaves tunnel facts available to the native source planner");
}

void CheckFootpathAccess(const Case &input,
                         std::optional<bool> expected,
                         const outshine::Generators::RegionSurfaceRecipes &templates,
                         bool invalid = false) {
  const std::array<std::string, 1> layers{"streets"};
  OsmField field(1, layers, input.Schema);
  CHECK(field.Accept(0, 0, Tile(input)).has_value(), "foot access comes from actual provider tags");
  StreetField streets;
  CHECK(streets.Ingest(field, templates) == 1, "the tagged path reaches the live source reader");
  const auto source = ReadFootpathSource(field, streets, 0);
  if (invalid) {
    CHECK(!source && source.error() == FootpathSourceError::InvalidAccess,
          "malformed supplied access is rejected rather than replaced by an assumption");
    return;
  }
  CHECK(source && source->FootAccess == expected,
        "supplied foot overrides access; absent stays absent");
  if (!source) { return; }
  CHECK(source->Kind == "steps", "the actual source classification reaches the native planner");
  CHECK(!source->Placement.Layer && !source->Placement.Bridge && !source->Placement.Tunnel,
        "the source reader does not invent placement facts");
  CHECK(!source->OwnsEnds[0] && source->OwnsEnds[1],
        "a tile-boundary end stays a fragment while the interior end belongs to the source");
  const OsmField unrelated(1, layers, input.Schema);
  CHECK(!ReadFootpathSource(unrelated, streets, 0), "an index cannot read another field's tags");
}

}

int main() {
  outshine::Generators::GroundMaterialCatalog materials;
  outshine::Generators::RegionSurfaceRecipes templates;
  CHECK(materials.Load("data/world/ground-materials.json"), "ground catalog loads");
  CHECK(templates.Load("data/world/vegetation.json", materials), "street rules load");
  if (!templates.Ready()) { return Report(); }
  const std::vector<Case> cases{
      {},
      {.Layer = Number(0),
       .Bridge = Number(0),
       .Tunnel = Number(0),
       .Expected = {.Layer = 0, .Bridge = false, .Tunnel = false}},
      {.Layer = Text("-2"),
       .Bridge = Text("yes"),
       .Tunnel = Text("no"),
       .Expected = {.Layer = -2, .Bridge = true, .Tunnel = false}},
      {.Bridge = Text("false"),
       .Tunnel = Text("true"),
       .Expected = {.Bridge = false, .Tunnel = true}},
      {.Schema = MvtSchema::OpenMapTiles, .Brunnel = "bridge", .Expected = {.Bridge = true}},
      {.Schema = MvtSchema::OpenMapTiles, .Brunnel = "tunnel", .Expected = {.Tunnel = true}},
      {.Schema = MvtSchema::OpenMapTiles,
       .Bridge = Number(0),
       .Brunnel = "bridge",
       .Expected = {.Bridge = false}},
      {.Layer = Text("1.5"), .Error = SurfacePlacementError::InvalidLayer},
      {.Bridge = Number(.5), .Error = SurfacePlacementError::InvalidBridge},
      {.Tunnel = Text("unknown"), .Error = SurfacePlacementError::InvalidTunnel},
      {.Bridge = Number(1),
       .Tunnel = Number(1),
       .Error = SurfacePlacementError::ConflictingStructure},
      {.Bridge = Number(std::numeric_limits<double>::infinity()),
       .Error = SurfacePlacementError::InvalidBridge}};
  for (const auto &input : cases) { CheckCase(input, templates); }
  CheckFootpathAccess({.Kind = "steps"}, std::nullopt, templates);
  CheckFootpathAccess({.Schema = MvtSchema::OpenMapTiles,
                       .Kind = "path",
                       .Subclass = "steps",
                       .Foot = Text("designated")},
                      true,
                      templates);
  CheckFootpathAccess({.Kind = "steps", .Access = Text("no")}, false, templates);
  CheckFootpathAccess(
      {.Kind = "steps", .Foot = Text("designated"), .Access = Text("no")}, true, templates);
  CheckFootpathAccess(
      {.Kind = "steps", .Foot = Number(0), .Access = Text("yes")}, false, templates);
  CheckFootpathAccess({.Kind = "steps", .Foot = Text("unknown"), .Access = Text("yes")},
                      std::nullopt,
                      templates,
                      true);
  return Report();
}
