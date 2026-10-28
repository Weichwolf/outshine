#include "MvtTransportSnapshot.h"
#include "MvtTransportSnapshotInternal.h"
#include "MvtTransportWireFixture.h"
#include "OsmField.h"
#include "StreetField.h"
#include "StreetGraphBuilder.h"
#include "GroundMaterialCatalog.h"
#include "RegionSurfaceRecipes.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <bit>
#include <stop_token>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <print>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
namespace Wire = outshine::Test::MvtTransportWire;

Wire::Bytes ThreeWays() {
  Wire::Bytes layer{Wire::kLayerVersion, 2, Wire::kLayerExtent, 64};
  Wire::Text(layer, 0x0a, "streets");
  for (const auto *key : {"kind", "layer"}) { Wire::Text(layer, Wire::kLayerKey, key); }
  for (const auto *text : {"path", "0"}) {
    Wire::Bytes value;
    Wire::Text(value, 0x0a, text);
    Wire::Message(layer, Wire::kLayerValue, value);
  }
  Wire::Message(
      layer,
      Wire::kFeatureTags,
      Wire::Feature({.Id = 42, .Tags = {0, 0, 1, 1}, .Geometry = {9,  16, 16, 34, 4,  0,  4,  8,
                                                                  4,  7,  4,  0,  9,  15, 32, 10,
                                                                  16, 0,  9,  15, 32, 10, 16, 0}}));
  Wire::Bytes tile;
  Wire::Message(tile, Wire::kLayerKey, layer);
  return tile;
}

void CheckMissingWay(const MvtTransportSourceFacts::MissingWay &missing,
                     const MvtTransportSnapshot &made,
                     const OsmField &field,
                     const StreetField &streets) {
  CHECK(missing.Omission < made.Omissions.size(), "missing geometry refers to canonical omission");
  if (missing.Omission >= made.Omissions.size()) { return; }
  const auto &omission = made.Omissions[missing.Omission];
  CHECK(omission.Fragment && omission.Reason == MvtTransportOmissionReason::MissingTopology,
        "geometry cannot qualify a feature-wide or unknown omission");
  const auto &way = streets.Ways()[missing.Way.StreetWay];
  CHECK(missing.Points.size() == way.PointCount &&
            missing.Way.HalfWidthWord == std::bit_cast<uint32_t>(way.HalfWidthM) &&
            missing.Way.OwnsEnds == way.OwnsEnds && missing.Layer == way.Layer &&
            missing.Bridge == way.Bridge && !missing.Tunnel,
        "full source shape, exact width, original ownership and vertical semantics survive");
  for (size_t i = 0; i < missing.Points.size(); ++i) {
    CHECK(missing.Points[i].LongitudeDeg == field.Points()[2 * (way.FirstPoint + i) + 1] &&
              missing.Points[i].LatitudeDeg == field.Points()[2 * (way.FirstPoint + i)],
          "every original point survives without XY reconstruction or endpoint reduction");
  }
  if (missing.Way.StreetWay == 0) {
    CHECK(missing.Points.size() == 5 &&
              std::ranges::none_of(missing.Way.EndStencil,
                                   [&](const auto &point) {
                                     return point.LongitudeDeg == missing.Points[2].LongitudeDeg &&
                                            point.LatitudeDeg == missing.Points[2].LatitudeDeg;
                                   }),
          "the curved missing way retains interior information absent from its end stencil");
  }
}

void CheckOriginalFacts(const MvtTransportSnapshot &made,
                        const MvtTransportSnapshot &full,
                        const OsmField &field,
                        const StreetField &streets,
                        const Path::Network &network) {
  CHECK(made.SourceFacts && full.SourceFacts, "bounded captures retain original omission facts");
  if (!made.SourceFacts || !full.SourceFacts) { return; }
  const auto &facts = *made.SourceFacts;
  CHECK(facts.Ways.size() == 1 && facts.MissingWays.size() == 2 &&
            full.SourceFacts->MissingWays.empty(),
        "only missing original ways add full geometry; represented ways retain existing facts");
  if (facts.MissingWays.size() != 2) { return; }
  for (const auto &missing : facts.MissingWays) { CheckMissingWay(missing, made, field, streets); }
  for (const auto changed : {0, 1, 2, 3, 4, 5}) {
    auto copy = facts;
    auto &missing = copy.MissingWays.front();
    if (changed == 0) { missing.Points[2].LatitudeDeg += 0.000001; }
    if (changed == 1) { missing.Omission ^= 1u; }
    if (changed == 2) { ++missing.Layer; }
    if (changed == 3) { missing.Bridge = !missing.Bridge; }
    if (changed == 4) { missing.Way.HalfWidthWord ^= 1u; }
    if (changed == 5) { copy.OmissionsDigest.front() ^= 1u; }
    MvtTransport::BindSourceFacts(copy);
    CHECK(copy.Digest != facts.Digest,
          "original omission geometry and binding affect source digest");
  }
  const auto limited = BuildMvtTransportSnapshot(
      field,
      streets,
      network,
      {},
      {},
      {.NodeGroupPayloadBytesMost = 1 << 20, .SourceFactsPayloadBytesMost = 1});
  CHECK(
      limited && !limited->SourceFacts &&
          limited->SourceFactState == MvtTransportSourceFactState::BudgetExceeded &&
          limited->Omissions.size() == made.Omissions.size(),
      "insufficient capture budget preserves original omissions and valid native partial network");
  std::stop_source cancelled;
  cancelled.request_stop();
  CHECK(!BuildMvtTransportSnapshot(
            field,
            streets,
            network,
            {},
            cancelled.get_token(),
            {.NodeGroupPayloadBytesMost = 1 << 20, .SourceFactsPayloadBytesMost = 1 << 20}),
        "cancelled capture never publishes partial original facts");
}
}

int main() {
  Generators::GroundMaterialCatalog materials;
  Generators::RegionSurfaceRecipes recipes;
  CHECK(materials.Load("data/world/ground-materials.json") &&
            recipes.Load("data/world/vegetation.json", materials),
        "actual source recipes load");
  if (!recipes.Ready()) { return Report(); }
  const std::array<std::string, 1> layers{"streets"};
  OsmField field(16, layers);
  CHECK(field.Accept(Wire::kTileX, Wire::kTileX, ThreeWays()),
        "one actual multipart MVT feature loads");
  StreetField streets;
  (void)streets.Ingest(field, recipes);
  CHECK(field.Features().size() == 1 && streets.Ways().size() == 3,
        "one feature produces three distinct source ways");
  if (streets.Ways().size() != 3) { return Report(); }
  const auto first = streets.SourceOf(field, 0);
  const auto second = streets.SourceOf(field, 1);
  CHECK(first && second && first->Feature == second->Feature && first->Ring != second->Ring,
        "the two absent ways have the same feature and different actual ring provenance");
  const auto fullGraph = StreetGraphBuilder::BuildOneShot(
      streets, field.Points(), [](LongitudeLatitude) -> std::optional<double> { return 0.0; });
  CHECK(fullGraph.Graph, "the complete source topology retains every original way");
  if (!fullGraph.Graph) { return Report(); }
  const std::array<GeographicBounds, 1> fullCoverage{
      {{.WestDeg = 0, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 0}}};
  const auto full = BuildMvtTransportSnapshot(
      field,
      streets,
      *fullGraph.Graph,
      fullCoverage,
      {},
      {.NodeGroupPayloadBytesMost = 1 << 20, .SourceFactsPayloadBytesMost = 1 << 20});
  CHECK(full, "complete normalization provides the existing source fragment identities");
  if (!full) { return Report(); }
  CHECK(full->Omissions.empty(), "the complete three-way input has no omitted topology");
  std::vector<std::array<uint8_t, 32>> expected;
  const auto &provenance = full->Network->Provenance();
  for (size_t path = 0; path < full->StreetWays.size(); ++path) {
    if (full->StreetWays[path] < 2) {
      expected.push_back(provenance.References[provenance.Paths[path].FirstReference].Fragment);
    }
  }
  std::ranges::sort(expected);
  const auto duplicate = std::ranges::unique(expected);
  expected.erase(duplicate.begin(), duplicate.end());
  CHECK(expected.size() == 2 && expected[0] != expected[1],
        "complete topology gives the absent source ways distinct existing identities");
  auto partial = streets.Without(0).Without(1);
  const auto graph = StreetGraphBuilder::BuildOneShot(
      partial, field.Points(), [](LongitudeLatitude) -> std::optional<double> { return 0.0; });
  CHECK(graph.Graph && graph.Graph->WayCount() == 1,
        "the native network contains exactly the third source way");
  if (!graph.Graph) { return Report(); }
  const std::array<GeographicBounds, 1> coverage{
      {{.WestDeg = 0, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 0}}};
  const auto made = BuildMvtTransportSnapshot(
      field,
      streets,
      *graph.Graph,
      coverage,
      {},
      {.NodeGroupPayloadBytesMost = 1 << 20, .SourceFactsPayloadBytesMost = 1 << 20});
  CHECK(made, "normalization preserves a valid partial native network");
  if (!made) { return Report(); }
  const auto missing = std::ranges::count(
      made->Omissions, MvtTransportOmissionReason::MissingTopology, &MvtTransportOmission::Reason);
  std::println("MISSING_WAY_SOURCE features={} ways={} network_ways={} missing_records={} "
               "omission_bytes={}",
               field.Features().size(),
               streets.Ways().size(),
               graph.Graph->WayCount(),
               missing,
               sizeof(MvtTransportOmission));
  CHECK(missing == 2, "both completely absent ways retain distinguishable omission records");
  std::vector<std::array<uint8_t, 32>> actual;
  for (const auto &omission : made->Omissions) {
    if (omission.Reason != MvtTransportOmissionReason::MissingTopology) { continue; }
    CHECK(omission.Fragment && omission.Feature == field.Features()[0].ContentDigest,
          "the actual feature and its specific missing fragment are both retained");
    if (omission.Fragment) { actual.push_back(*omission.Fragment); }
  }
  std::ranges::sort(actual);
  CHECK(actual == expected, "omission identities match the original complete network exactly");
  CheckOriginalFacts(*made, *full, field, streets, *graph.Graph);
  return Report();
}
