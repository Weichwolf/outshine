#include "Check.h"
#include "OsmXmlReader.h"
#include "RoadAlignmentBuildQueue.h"
#include "RoadHeightCoverage.h"

#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Test;
  using namespace outshine::World;

  std::ifstream input("src/assets/world/osm/HockenheimringGrandPrix.osm", std::ios::binary);
  CHECK(input.good(), "the pinned Hockenheim source exists");
  if (!input) { return Report(); }
  const std::string xml(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
  auto elements = OsmXmlReader::Read(xml, {.DatasetId = "openstreetmap", .Revision = "pin-r1"});
  CHECK(elements.has_value(), "the route source parses");
  if (!elements) { return Report(); }
  auto topology = TransportTopology::Build(*elements);
  CHECK(topology.has_value(), "the route topology builds");
  if (!topology) { return Report(); }
  auto circuit = topology->ResolveCircuit(*elements, 284588);
  CHECK(circuit.has_value(), "the source circuit resolves");
  if (!circuit) { return Report(); }
  ResolvedTransport transport{.Graph = std::move(*topology),
                              .Routes = {{.Id = "grand-prix", .Circuit = std::move(*circuit)}}};
  auto source = std::make_shared<const TransportNetworkSnapshot>(std::move(*elements),
                                                                 std::move(transport),
                                                                 std::vector<SourceCoverage>{},
                                                                 TransportLoadMetrics{});
  const auto coverage =
      RoadHeightCoverage::Select(*source, {.Zoom = 15, .MaximumEdges = 512, .MaximumTiles = 256});
  CHECK(coverage && coverage->SelectedRouteIndices == std::vector<size_t>{0} &&
            coverage->Tiles.size() > 1,
        "the worker input contains one admitted multi-tile route");
  if (!coverage) { return Report(); }

  std::vector<SourcedTerrainFields::Entry> fields;
  for (const TileId tile : coverage->Tiles) {
    auto field = std::make_shared<Ground::TerrainField>(2, 2);
    for (uint32_t row = 0; row < 2; ++row) {
      for (uint32_t col = 0; col < 2; ++col) { field->SetM(row, col, 100.0f); }
    }
    field->AddSource({.Kind = DataKind::Elevation,
                      .Tile = tile,
                      .SourceId = "controlled-dem",
                      .Revision = "r1"});
    fields.emplace_back(tile, std::move(field));
  }

  Tasks tasks(1);
  RoadAlignmentBuildQueue queue;
  const auto submit = [&](uint64_t generation, std::vector<SourcedTerrainFields::Entry> terrain) {
    return queue.TryStart(
        tasks,
        {.Source = source,
         .Terrain = SourcedTerrainFields(std::move(terrain)),
         .RouteIndices = coverage->SelectedRouteIndices,
         .RenderFrame = TangentFrame::At({.LongitudeDeg = 8.565, .LatitudeDeg = 49.329}),
         .TerrainZoom = 15,
         .CandidateGeneration = generation});
  };
  const auto finish = [&](uint64_t activeGeneration) {
    for (int attempt = 0; attempt < 50 && queue.Busy(); ++attempt) {
      (void)tasks.AwaitCompletion(0.1);
      queue.Poll(tasks, activeGeneration);
    }
    CHECK(!queue.Busy(), "a bounded worker finishes within five seconds");
  };

  CHECK(submit(41, fields), "the first candidate starts on the worker");
  finish(41);
  auto built = queue.TakeCompleted();
  CHECK(built && *built && built->value().Matches(41, source->SourceIdentity()) &&
            built->value().Routes.size() == 1 &&
            built->value().Routes.front().Alignment->Closed() &&
            built->value().Routes.front().Alignment->Edges().size() == 267 &&
            built->value().Routes.front().Surface &&
            built->value().Routes.front().Surface->SurfaceGeometry.wellFormed() &&
            built->value().Earthworks.size() ==
                (built->value().Routes.front().Surface->Spans.size() + 4) / 5,
        "the worker returns one native road surface and all source edges with provenance");

  CHECK(submit(42, fields), "a later candidate starts after the first result is taken");
  finish(43);
  CHECK(!queue.TakeCompleted(), "a completed obsolete candidate cannot land");

  fields.pop_back();
  CHECK(submit(44, std::move(fields)), "the missing-DEM candidate starts");
  finish(44);
  auto missing = queue.TakeCompleted();
  CHECK(missing && !*missing &&
            missing->error().Code == RoadAlignmentBuildErrorCode::MissingTerrain &&
            missing->error().Tile == coverage->Tiles.back() &&
            missing->error().RouteId == "grand-prix",
        "missing sourced terrain fails with route and tile identity instead of a fallback");
  CHECK(queue.TryStart(
            tasks,
            {.Source = source,
             .Terrain = SourcedTerrainFields(std::vector<SourcedTerrainFields::Entry>{}),
             .RouteIndices = {0, 0},
             .RenderFrame = TangentFrame::At({.LongitudeDeg = 8.565, .LatitudeDeg = 49.329}),
             .TerrainZoom = 15,
             .CandidateGeneration = 45}),
        "a malformed request reaches the bounded worker validator");
  finish(45);
  auto duplicate = queue.TakeCompleted();
  CHECK(duplicate && !*duplicate &&
            duplicate->error().Code == RoadAlignmentBuildErrorCode::InvalidRequest,
        "the same source route cannot consume the candidate budget twice");
  return Report();
}
