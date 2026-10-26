#include "MvtTransportSnapshot.h"
#include "MvtTransportWireFixture.h"
#include "OsmField.h"
#include "StreetField.h"
#include "StreetGraphBuilder.h"
#include "GroundMaterialCatalog.h"
#include "RegionSurfaceRecipes.h"
#include "TransportPlanCodec.h"
#include "TransportCoreSelection.h"
#include "Sha256.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <numeric>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
constexpr TransportPlanCodecLimits kLimits{.PackageBytesMost = 65536,
                                           .DecodedCapacityMost = 131072};

std::optional<std::vector<uint8_t>> BuildArchive() {
  GroundMaterialCatalog materials;
  RegionSurfaceRecipes recipes;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            recipes.Load("src/assets/world/vegetation.json", materials),
        "the actual source classification recipes are available");
  const std::array<std::string, 1> layers{"streets"};
  OsmField field(16, layers);
  const auto tile = MvtTransportWire::Tile();
  CHECK(field.Accept(MvtTransportWire::kTileX, MvtTransportWire::kTileX, tile),
        "the native producer receives actual MVT bytes");
  StreetField streets;
  (void)streets.Ingest(field, recipes);
  const auto graph = StreetGraphBuilder::BuildOneShot(
      streets, field.Points(), [](LongitudeLatitude) -> std::optional<double> { return 0.0; });
  CHECK(graph.Graph != nullptr, "the existing topology has been built once");
  if (!graph.Graph) { return std::nullopt; }
  const std::array<GeographicBounds, 1> coverage{
      {{.WestDeg = 0, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 0}}};
  const auto normalized = BuildMvtTransportSnapshot(field, streets, *graph.Graph, coverage);
  CHECK(normalized && normalized->Network->Provenance().Sources.size() == 1,
        "normalization supplies the actual native source row");
  if (!normalized || normalized->Network->Provenance().Sources.size() != 1) { return std::nullopt; }
  const auto &source = normalized->Network->Provenance().Sources.front();
  CHECK(source.Content == Sha256Digest(tile.data(), tile.size()),
        "the stored source digest is the actual wire input, not a synthetic product identifier");
  constexpr uint32_t halfTile = static_cast<uint32_t>(MvtTransportWire::kTileX) * 2;
  TransportPlan plan({.Core = {.Level = 17, .X = halfTile + 1, .Y = halfTile - 1}});
  const auto bounds = plan.Definition.Core.Bounds();
  if (!bounds) { return std::nullopt; }
  plan.Anchor = {.LongitudeDeg = std::midpoint(bounds->WestDeg, bounds->EastDeg),
                 .LatitudeDeg = std::midpoint(bounds->SouthDeg, bounds->NorthDeg)};
  plan.Coverage = TransportCoverage::Partial;
  plan.Omissions = {{.SourceId = source.Item, .Reason = TransportOmissionReason::MissingBoundary}};
  plan.Network = normalized->Network;
  plan.Inputs = {{.DatasetId = source.Identity.DatasetId,
                  .Version = "normalized-mvt-transport-1",
                  .Revision = source.Identity.Revision,
                  .ItemId = source.Item,
                  .Content = source.Content}};
  const auto binding = TransportPlanInputBinding(plan.Inputs, kLimits.PackageBytesMost);
  if (!binding) { return std::nullopt; }
  plan.InputBinding = *binding;
  const auto bytes = EncodeTransportPlan(plan, kLimits);
  CHECK(bytes.has_value(),
        "the partial archive stores native topology with its real input binding");
  if (!bytes) { return std::nullopt; }
  return *bytes;
}
}

int main() {
  const auto bytes = BuildArchive();
  CHECK(bytes.has_value(), "the source producer has retired before the archive consumer runs");
  if (!bytes) { return Report(); }
  const auto plan = DecodeTransportPlan(*bytes, kLimits);
  CHECK(plan && plan->Network && plan->Coverage == TransportCoverage::Partial &&
            plan->Paths.empty() && plan->Omissions.size() == 1,
        "unqualified geometry remains explicitly partial after source retirement");
  if (!plan || !plan->Network) { return Report(); }
  const auto &provenance = plan->Network->Provenance();
  CHECK(provenance.IdentitySpace == World::TransportIdentitySpace::DerivedFragments &&
            provenance.Sources.size() == 1 && provenance.Paths.size() == 4 &&
            provenance.References.size() == 4 && plan->Network->Topology().Edges().size() == 6,
        "stored MVT identity, directed topology and complete reference spans survive");
  const auto selected = SelectTransportCore(plan->Definition.Core,
                                            plan->Network,
                                            {.SourceNodesMost = 16,
                                             .SourceEdgesMost = 16,
                                             .SegmentsMost = 16,
                                             .ContainerCapacityMost = 4096});
  CHECK(selected && !selected->Segments.empty(),
        "the geographic consumer selects stored native data without the retired source fields");
  CHECK(EncodeTransportPlan(*plan, kLimits) == *bytes,
        "source-free reconstruction reencodes the entire actual native archive identically");
  return Report();
}
