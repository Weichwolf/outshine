#include "Check.h"
#include "CppAllocationProbe.h"
#include "GroundMaterialCatalog.h"
#include "GroundPortSources.h"
#include "MvtGroundPortInputs.h"
#include "MvtTransportSnapshot.h"
#include "MvtTransportWireFixture.h"
#include "OsmField.h"
#include "RegionSurfaceRecipes.h"
#include "StreetField.h"
#include "StreetGraphBuilder.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <print>
#include <ratio>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
namespace Wire = MvtTransportWire;

struct SourceInput {
  std::string_view Kind = "path";
  std::optional<std::string_view> Indoor = std::nullopt;
  std::optional<std::string_view> Level = std::nullopt;
  bool Reverse = false;
  bool Second = false;
};

Wire::Bytes SourceTile(const SourceInput &input) {
  Wire::Bytes layer{Wire::kLayerVersion, 2, Wire::kLayerExtent, 64};
  Wire::Text(layer, 0x0a, "streets");
  for (const auto *key : {"kind", "indoor", "level", "oneway"}) {
    Wire::Text(layer, Wire::kLayerKey, key);
  }
  Wire::Bytes tags{0, 0};
  uint8_t next = 0;
  const auto value = [&](std::string_view text) {
    Wire::Bytes encoded;
    Wire::Text(encoded, 0x0a, text);
    Wire::Message(layer, Wire::kLayerValue, encoded);
    return next++;
  };
  (void)value(input.Kind);
  if (input.Indoor) { tags.insert(tags.end(), {1, value(*input.Indoor)}); }
  if (input.Level) { tags.insert(tags.end(), {2, value(*input.Level)}); }
  if (input.Reverse) { tags.insert(tags.end(), {3, value("-1")}); }
  Wire::Message(
      layer,
      Wire::kFeatureTags,
      Wire::Feature(
          {.Id = 11, .Tags = tags, .Geometry = {9, 16, 32, Wire::kLineToTwo, 32, 0, 32, 0}}));
  if (input.Second) {
    const auto kind = value("service");
    Wire::Message(layer,
                  Wire::kFeatureTags,
                  Wire::Feature({.Id = 12, .Tags = {0, kind}, .Geometry = {9, 16, 64, 10, 64, 0}}));
  }
  Wire::Bytes tile;
  Wire::Message(tile, Wire::kLayerKey, layer);
  return tile;
}

std::expected<MvtTransportSnapshot, std::string> Normalize(OsmField &field,
                                                           StreetField &streets,
                                                           const RegionSurfaceRecipes &recipes,
                                                           const SourceInput &input) {
  const auto accepted = field.Accept(Wire::kTileX, Wire::kTileX, SourceTile(input));
  if (!accepted) { return std::unexpected("controlled MVT source was rejected"); }
  (void)streets.Ingest(field, recipes);
  const auto graph = StreetGraphBuilder::BuildOneShot(
      streets, field.Points(), [](LongitudeLatitude) -> std::optional<double> { return 0.0; });
  if (!graph.Graph) { return std::unexpected("native source graph was not built"); }
  constexpr std::array<GeographicBounds, 1> kCoverage{
      {{.WestDeg = -1, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 1}}};
  return BuildMvtTransportSnapshot(field, streets, *graph.Graph, kCoverage);
}

bool SameProfiles(const MvtGroundPortInputs &a, const MvtGroundPortInputs &b) {
  return a.Profiles.size() == b.Profiles.size() &&
         std::ranges::equal(a.Profiles, b.Profiles, [](const auto &left, const auto &right) {
           return left.PathId == right.PathId && left.MaximumGradient == right.MaximumGradient;
         });
}

void CheckBinding(const MvtTransportSnapshot &snapshot,
                  const OsmField &field,
                  const StreetField &streets,
                  const MvtGroundPortInputs &inputs) {
  CHECK(inputs.Network == snapshot.Network && inputs.UnknownNodes.empty(),
        "owned ground inputs pin the existing graph without copying or inventing nodes");
  const auto &paths = snapshot.Network->Provenance().Paths;
  CHECK(inputs.Profiles.size() == paths.size() && !paths.empty(),
        "every native path receives its actually available profile");
  for (size_t at = 0; at < inputs.Profiles.size(); ++at) {
    const auto &way = streets.Ways()[snapshot.StreetWays[at]];
    CHECK(inputs.Profiles[at].PathId == paths[at].PathId &&
              inputs.Profiles[at].MaximumGradient == static_cast<double>(way.MaxGradient),
          "profile identity and native binary32 gradient survive exactly");
  }
  const auto enlarged = BindMvtGroundPortInputs(
      snapshot,
      field,
      streets,
      {.Nodes = 65536, .DirectedEdges = 65536, .Paths = 65536, .ContainerBytes = 8388608});
  CHECK(enlarged && SameProfiles(inputs, *enlarged) &&
            enlarged->ProfileInput.Content == inputs.ProfileInput.Content,
        "admission caps do not change profiles or semantic input binding");
  auto sources = GroundPortSources::Make(inputs.Network, inputs.Profiles, inputs.UnknownNodes);
  CHECK(sources.has_value(), "actual MVT inputs construct the existing bounded ground index");
  if (!sources) { return; }
  const auto edges = snapshot.Network->Topology().Edges();
  for (const auto &edge : edges) {
    const auto chain = sources->Chain(edge.Id);
    CHECK(chain && chain->MaximumGradient == static_cast<double>(streets.Ways()[0].MaxGradient) &&
              chain->WidthM == edge.WidthM && chain->Source == snapshot.Network,
          "each original seed and direction consumes the same complete source component");
  }
}

void CheckGuards(const MvtTransportSnapshot &snapshot,
                 const OsmField &field,
                 const StreetField &streets) {
  BeginCppAllocationProbe();
  const auto capped = BindMvtGroundPortInputs(snapshot, field, streets, {.Paths = 0});
  const auto cost = EndCppAllocationProbe();
  CHECK(!capped && capped.error() == MvtGroundPortInputError::CapacityExceeded && cost.Calls == 0,
        "path admission fails before temporary allocation or DEM access");
  CHECK(!BindMvtGroundPortInputs(snapshot, field, streets, {.ContainerBytes = 0}) &&
            !BindMvtGroundPortInputs(snapshot, field, streets, {.AttributeWords = 0}),
        "owned memory and attribute work each have an independent finite cap");
  std::stop_source stop;
  stop.request_stop();
  CHECK(!BindMvtGroundPortInputs(snapshot, field, streets, {}, stop.get_token()),
        "pre-cancellation publishes no partially bound input");
  const auto retired = streets.Without(snapshot.StreetWays.front());
  const auto lost = BindMvtGroundPortInputs(snapshot, field, retired);
  CHECK(!lost && lost.error() == MvtGroundPortInputError::SourceChanged,
        "retired ways cannot reuse a source binding of the same size");
  auto swapped = snapshot;
  const auto different = std::ranges::find_if(
      swapped.StreetWays, [&](uint32_t way) { return way != swapped.StreetWays.front(); });
  if (different != swapped.StreetWays.end()) {
    std::swap(swapped.StreetWays.front(), *different);
    const auto wrong = BindMvtGroundPortInputs(swapped, field, streets);
    CHECK(!wrong && wrong.error() == MvtGroundPortInputError::InvalidBinding,
          "equal-size swapped Way indices cannot substitute another profile or fragment");
  }
  auto omitted = snapshot;
  const auto original = streets.SourceOf(field, snapshot.StreetWays.front());
  CHECK(original.has_value(), "the real source feature remains accessible during extraction");
  if (!original) { return; }
  const auto &path = snapshot.Network->Provenance().Paths.front();
  const auto source = snapshot.Network->Provenance().References[path.FirstReference].Source;
  omitted.Omissions.push_back({.Source = source,
                               .Feature = field.Features()[original->Feature].ContentDigest,
                               .Reason = MvtTransportOmissionReason::MissingTopology});
  const auto unknown = BindMvtGroundPortInputs(omitted, field, streets);
  CHECK(unknown && !unknown->UnknownNodes.empty(),
        "an omitted source fragment makes its surviving component endpoints unknown");
  if (unknown) {
    auto index =
        GroundPortSources::Make(unknown->Network, unknown->Profiles, unknown->UnknownNodes);
    CHECK(index.has_value(), "unknown endpoints remain representable in the native source index");
    if (index) {
      const auto chain = index->Chain(snapshot.Network->Topology().Edges().front().Id);
      CHECK(!chain && chain.error() == GroundPortSourceError::UnknownFrontier,
            "an incomplete source component stays Pending before terrain work");
    }
  }
  omitted.Omissions.back().Feature.fill(0);
  const auto unresolved = BindMvtGroundPortInputs(omitted, field, streets);
  CHECK(!unresolved && unresolved.error() == MvtGroundPortInputError::MissingIncidence,
        "a missing feature with no retained incidence cannot be silently called complete");
}

void CheckOmittedCrossing(const RegionSurfaceRecipes &recipes) {
  const std::array<std::string, 1> layers{"streets"};
  OsmField field(16, layers);
  CHECK(field.Accept(Wire::kTileX, Wire::kTileX, Wire::Tile()).has_value(),
        "the existing cross-domain MVT wire fixture loads");
  StreetField streets;
  (void)streets.Ingest(field, recipes);
  const auto graph = StreetGraphBuilder::BuildOneShot(
      streets, field.Points(), [](LongitudeLatitude) -> std::optional<double> { return 0.0; });
  CHECK(graph.Graph != nullptr, "the existing cross-domain source graph builds");
  if (!graph.Graph) { return; }
  constexpr std::array<GeographicBounds, 1> kCoverage{
      {{.WestDeg = -1, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 1}}};
  const auto snapshot = BuildMvtTransportSnapshot(field, streets, *graph.Graph, kCoverage);
  CHECK(snapshot.has_value(), "native normalization retains actual cross-domain omissions");
  if (!snapshot) { return; }
  const auto inputs = BindMvtGroundPortInputs(*snapshot, field, streets);
  CHECK(inputs && inputs->UnknownNodes.size() == snapshot->Network->Topology().Nodes().size(),
        "actual unqualified level connections never become known ground component ends");
  if (!inputs) { return; }
  auto index = GroundPortSources::Make(inputs->Network, inputs->Profiles, inputs->UnknownNodes);
  CHECK(index.has_value(), "actual omitted topology remains representable without joining domains");
  if (!index) { return; }
  for (const auto &edge : inputs->Network->Topology().Edges()) {
    const auto chain = index->Chain(edge.Id);
    CHECK(!chain && chain.error() == GroundPortSourceError::UnknownFrontier,
          "each unqualified domain stays Pending before height work");
  }
}

void CheckMissing(const RegionSurfaceRecipes &recipes, const SourceInput &input) {
  const std::array<std::string, 1> layers{"streets"};
  OsmField field(16, layers);
  StreetField streets;
  const auto snapshot = Normalize(field, streets, recipes, input);
  CHECK(snapshot.has_value(), "profile counterexample reaches actual native normalization");
  if (!snapshot) { return; }
  const auto inputs = BindMvtGroundPortInputs(*snapshot, field, streets);
  CHECK(inputs && inputs->Profiles.empty() && inputs->Work.MissingProfiles > 0,
        "missing class gradients and non-ground profiles receive no invented fallback");
  if (!inputs) { return; }
  auto index = GroundPortSources::Make(inputs->Network, inputs->Profiles, inputs->UnknownNodes);
  CHECK(index.has_value(), "missing profiles preserve the usable source graph");
  if (!index || inputs->Network->Topology().Edges().empty()) { return; }
  const auto chain = index->Chain(inputs->Network->Topology().Edges().front().Id);
  CHECK(!chain && chain.error() == GroundPortSourceError::MissingProfile,
        "profile absence is an explicit Pending reason before DEM acquisition");
}
}

int main() {
  GroundMaterialCatalog materials;
  RegionSurfaceRecipes recipes;
  CHECK(materials.Load("data/world/ground-materials.json") &&
            recipes.Load("data/world/vegetation.json", materials),
        "actual world recipes load");
  if (!recipes.Ready()) { return Report(); }
  std::optional<MvtGroundPortInputs> retained;
  const std::array<std::string, 1> layers{"streets"};
  {
    OsmField field(16, layers);
    StreetField streets;
    const auto snapshot =
        Normalize(field, streets, recipes, {.Indoor = "no", .Level = "0", .Reverse = true});
    CHECK(snapshot.has_value(), "controlled reverse-only MVT reaches native topology");
    if (!snapshot) { return Report(); }
    BeginCppAllocationProbe();
    const auto began = std::chrono::steady_clock::now();
    auto inputs = BindMvtGroundPortInputs(*snapshot, field, streets);
    const auto counts = EndCppAllocationProbe();
    const double elapsed =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    CHECK(inputs.has_value(), "actual source-bound profile extraction succeeds");
    if (!inputs) { return Report(); }
    std::println(
        "MVT_GROUND_INPUTS paths={} edges={} tiles={} attribute_words={} capacity_bytes={} "
        "allocation_calls={} requested_bytes={} extraction_ms={}",
        inputs->Work.Paths,
        inputs->Work.DirectedEdges,
        inputs->Work.SourceTiles,
        inputs->Work.AttributeWords,
        inputs->Work.ContainerCapacityBytes,
        counts.Calls,
        counts.Bytes,
        elapsed);
    CheckBinding(*snapshot, field, streets, *inputs);
    CheckGuards(*snapshot, field, streets);
    retained = std::move(*inputs);
    CHECK(field.Accept(Wire::kNextTileX, Wire::kTileX, Wire::EmptyTile()).has_value(),
          "source generation can advance after extraction");
    CHECK(!BindMvtGroundPortInputs(*snapshot, field, streets),
          "stale source generations cannot be rebound even with the old Way count");
  }
  auto released =
      GroundPortSources::Make(retained->Network, retained->Profiles, retained->UnknownNodes);
  CHECK(released.has_value(), "owned scalar profiles and graph survive all source storage release");
  if (released) {
    const auto chain = released->Chain(retained->Network->Topology().Edges().front().Id);
    CHECK(chain.has_value(), "actual source geometry remains usable after source storage release");
  }
  CheckOmittedCrossing(recipes);
  CheckMissing(recipes, {.Kind = "rail"});
  CheckMissing(recipes, {.Indoor = "yes"});
  CheckMissing(recipes, {.Level = "1"});
  {
    OsmField field(16, layers);
    StreetField streets;
    const auto snapshot = Normalize(field, streets, recipes, {.Second = true});
    CHECK(snapshot.has_value(), "two distinct real source profiles normalize together");
    if (snapshot) { CheckGuards(*snapshot, field, streets); }
  }
  return Report();
}
