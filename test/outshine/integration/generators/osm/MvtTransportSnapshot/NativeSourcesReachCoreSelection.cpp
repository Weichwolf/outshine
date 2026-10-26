#include "MvtTransportSnapshot.h"
#include "MvtTransportWireFixture.h"
#include "OsmField.h"
#include "StreetField.h"
#include "StreetGraphBuilder.h"
#include "GroundMaterials.h"
#include "VegetationTemplates.h"
#include "TransportCoreSelection.h"
#include "BridgeInputRequirements.h"
#include "Corridors.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Generators::Osm;
using namespace outshine::Test::MvtTransportWire;

void CheckPlanningInput(const MvtTransportSnapshot &made,
                        const OsmField &field,
                        const StreetField &streets,
                        const Ground::GroundMaterials &materials,
                        const Ground::VegetationTemplates &recipes,
                        const Path::Network &graph) {
  using namespace outshine::Test;
  constexpr uint32_t kHalfTile = static_cast<uint32_t>(kTileX) * 2;
  const auto core = SelectTransportCore({.Level = 17, .X = kHalfTile + 1, .Y = kHalfTile - 1},
                                        made.Network,
                                        {.SourceNodesMost = 16,
                                         .SourceEdgesMost = 16,
                                         .SegmentsMost = 16,
                                         .ContainerCapacityMost = 4096});
  CHECK(core && !core->Segments.empty(),
        "native geographic demand contains original source segments");
  if (!core || core->Segments.empty()) { return; }
  std::vector<World::TransportEdgeId> edges;
  size_t bridges = 0;
  for (const auto &segment : core->Segments) {
    const auto &edge = made.Network->Topology().Edges()[segment.SourceEdge];
    bridges += static_cast<size_t>(edge.Bridge);
    edges.push_back(edge.Id);
  }
  CHECK(bridges == 0, "the chosen eastern core excludes the western bridge");
  const auto selected = ResolveMvtTransportWays(made, field, streets, edges);
  CHECK(selected && selected->size() == 1,
        "actual native core demand resolves one original street");
  if (!selected) { return; }
  const auto frame = TangentFrame::At(core->Segments.front().Ends.front().Geographic);
  const auto surface = TriangleBvh::Over({}, {});
  const Drape drape{.Surface = surface, .Field = [](EastNorth at) -> std::optional<double> {
                      static_cast<void>(at);
                      return 0.0;
                    }};
  const std::shared_ptr<const TerrainClassificationMap> classes;
  const Corridors::Site site{.Vectors = &field,
                             .Ways = streets,
                             .Materials = materials,
                             .Vegetation = recipes,
                             .Network = &graph,
                             .Standing = frame,
                             .Draped = drape,
                             .Classes = classes,
                             .WaterUpM = {},
                             .CensusAt = {}};
  CHECK(Corridors::BeginFunctionalPlan(site, *selected),
        "native geographic demand enters the existing functional planner with the actual source "
        "lease");
}

std::expected<MvtTransportSnapshot, std::string> Snapshot() {
  Ground::GroundMaterials materials;
  Ground::VegetationTemplates recipes;
  if (!materials.Load("src/assets/world/ground-materials.json") ||
      !recipes.Load("src/assets/world/vegetation.json", materials)) {
    return std::unexpected("transport recipes unavailable");
  }
  const std::array<std::string, 1> layers{"streets"};
  OsmField field(16, layers);
  if (!field.Accept(kTileX, kTileX, Test::MvtTransportWire::Tile())) {
    return std::unexpected("MVT input rejected");
  }
  StreetField streets;
  (void)streets.Ingest(field, recipes);
  const auto graph = StreetGraphBuilder::BuildOneShot(
      streets, field.Points(), [](LongitudeLatitude) -> std::optional<double> { return 0.0; });
  if (!graph.Graph) { return std::unexpected("source graph unavailable"); }
  const auto bounds = GeoCellId{.Level = 16, .X = kTileX, .Y = kTileX - 1}.Bounds();
  if (!bounds) { return std::unexpected("source bounds unavailable"); }
  auto made = BuildMvtTransportSnapshot(field, streets, *graph.Graph, std::span(&*bounds, 1));
  if (made) { CheckPlanningInput(*made, field, streets, materials, recipes, *graph.Graph); }
  return made;
}

bool SharesBoundary(const TransportCoreEnd &from, const TransportCoreEnd &to) {
  if (from.Geographic != to.Geographic) { return false; }
  return std::ranges::any_of(from.Boundaries, [&to](const auto &boundary) {
    return boundary && std::ranges::find(to.Boundaries, boundary) != to.Boundaries.end();
  });
}

size_t MatchingEnds(const TransportCoreSegment &a, const TransportCoreSegment &b) {
  size_t count = 0;
  for (const auto &from : a.Ends) {
    for (const auto &to : b.Ends) { count += static_cast<size_t>(SharesBoundary(from, to)); }
  }
  return count;
}

void CheckBridge(const MvtTransportSnapshot &made) {
  using namespace outshine::Test;
  const auto edges = made.Network->Topology().Edges();
  const auto seed = std::ranges::find_if(edges, [](const auto &edge) { return edge.Bridge; });
  CHECK(seed != edges.end(), "the actual MVT snapshot contains a bridge seed");
  if (seed == edges.end()) { return; }
  const std::array seeds{seed->Id};
  const auto closure = SelectBridgeInputRequirements(
      made.Network, seeds, {.SourceEdgesMost = 16, .ContainerCapacityMost = 4096});
  CHECK(closure.has_value(), "native bridge selection accepts MVT-derived identities");
  if (!closure) { return; }
  CHECK(
      closure->BridgeEdges.size() == 2 && closure->GroundNodes.empty() &&
          !closure->Unqualified.empty(),
      "unqualified MVT bridge ends remain unresolved rather than becoming invented ground anchors");
}
}

int main() {
  using namespace outshine::Test;
  const auto made = Snapshot();
  CHECK(made.has_value(),
        made ? "source owners can retire before native selection" : made.error().c_str());
  if (!made) { return Report(); }
  constexpr uint32_t kHalfTile = static_cast<uint32_t>(kTileX) * 2;
  constexpr uint32_t kSouthernRow = kHalfTile - 1;
  const TransportCoreLimits limits{.SourceNodesMost = 16,
                                   .SourceEdgesMost = 16,
                                   .SegmentsMost = 16,
                                   .ContainerCapacityMost = 4096};
  const auto left =
      SelectTransportCore({.Level = 17, .X = kHalfTile, .Y = kSouthernRow}, made->Network, limits);
  const auto right = SelectTransportCore(
      {.Level = 17, .X = kHalfTile + 1, .Y = kSouthernRow}, made->Network, limits);
  CHECK(left && right, "adjacent geographic cores consume the same native MVT product");
  if (!left || !right) { return Report(); }
  CHECK(!left->Segments.empty() && !right->Segments.empty(),
        "both cores retain actual source segments");
  size_t matchingBoundaries = 0;
  for (const auto &a : left->Segments) {
    const auto &edge = made->Network->Topology().Edges()[a.SourceEdge];
    for (const auto &b : right->Segments) {
      if (a.SourceEdge != b.SourceEdge) { continue; }
      CHECK(edge.Id.Direction == World::EdgeDirection::Reverse && !edge.Bridge,
            "the shared cut belongs to the actual reverse-only source road");
      matchingBoundaries += MatchingEnds(a, b);
    }
  }
  CHECK(matchingBoundaries == 1, "the two cores share exactly one canonical source-bound cut");
  CheckBridge(*made);
  return Report();
}
