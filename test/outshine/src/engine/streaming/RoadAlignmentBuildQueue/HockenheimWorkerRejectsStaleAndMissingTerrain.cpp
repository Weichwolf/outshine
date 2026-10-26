#include "HockenheimTransportFixture.h"
#include "HockenheimCircuitFixture.h"
#include "OsmTransportBuilder.h"
#include "Check.h"
#include "OsmXmlReader.h"
#include "RoadAlignmentBuildQueue.h"
#include "RoadHeightCoverage.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <print>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace Circuit = outshine::Test::HockenheimFixture;

namespace {
constexpr uint64_t kFirstCandidate = 41;
constexpr uint64_t kStaleCandidate = 42;
constexpr uint64_t kCurrentCandidate = 43;
constexpr uint64_t kMissingCandidate = 44;
constexpr uint64_t kDuplicateCandidate = 45;
constexpr outshine::LongitudeLatitude kFrameAnchor{.LongitudeDeg = 8.565, .LatitudeDeg = 49.329};
constexpr int kMostPolls = 50;
constexpr double kPollWaitS = 0.1;

std::vector<outshine::Generators::TerrainFieldSet::Entry>
MakeTerrainFields(std::span<const outshine::Data::TileId> tiles) {
  using namespace outshine;
  using namespace outshine::Data;
  std::vector<Generators::TerrainFieldSet::Entry> fields;
  for (const TileId tile : tiles) {
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
  return fields;
}

void DescribeResult(const std::optional<outshine::RoadAlignmentBuildQueue::Result> &built,
                    const outshine::Data::SourceIdentity &identity) {
  if (built && *built) {
    const auto &product = built->value();
    std::println(stderr,
                 "candidate matches={:d} routes={} earthworks={}",
                 static_cast<int>(product.Matches(kFirstCandidate, identity)),
                 product.Routes.size(),
                 product.Earthworks.size());
    for (const auto &route : product.Routes) {
      std::println(stderr,
                   "route closed={:d} edges={} surface={:d} valid={:d} spans={}",
                   static_cast<int>(route.Alignment && route.Alignment->Closed()),
                   route.Alignment ? route.Alignment->Edges().size() : 0,
                   static_cast<int>(static_cast<bool>(route.Surface)),
                   static_cast<int>(route.Surface && route.Surface->SurfaceGeometry.wellFormed()),
                   route.Surface ? route.Surface->Spans.size() : 0);
    }
  } else if (built) {
    std::println(stderr,
                 "road worker error={} route={}",
                 static_cast<int>(built->error().Code),
                 built->error().RouteId);
  }
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Test;
  using namespace outshine::World;

  auto fixture = Circuit::LoadNetwork();
  CHECK(fixture,
        fixture ? "pinned source, topology and directed circuit load" : fixture.error().c_str());
  if (!fixture) { return Report(); }
  auto &topology = fixture->Graph;
  auto &circuit = fixture->Route;

  ResolvedTransport transport{.Graph = std::move(topology),
                              .Routes = {{.Id = "grand-prix", .Circuit = std::move(circuit)}}};
  auto source = std::make_shared<const TransportNetworkSnapshot>(
      std::move(transport), std::vector<GeographicBounds>{}, TransportLoadMetrics{});
  const auto coverage =
      RoadHeightCoverage::Select(*source, {.Zoom = 15, .MaximumEdges = 512, .MaximumTiles = 256});
  CHECK(coverage && coverage->SelectedRouteIndices == std::vector<size_t>{0} &&
            coverage->Tiles.size() > 1,
        "the worker input contains one admitted multi-tile route");
  if (!coverage) { return Report(); }

  auto fields = MakeTerrainFields(coverage->Tiles);

  Tasks tasks(1);
  RoadAlignmentBuildQueue queue;
  const auto submit = [&](uint64_t generation,
                          std::vector<Generators::TerrainFieldSet::Entry> terrain) {
    return queue.TryStart(tasks,
                          {.Source = source,
                           .Terrain = Generators::TerrainFieldSet(std::move(terrain)),
                           .RouteIndices = coverage->SelectedRouteIndices,
                           .RenderFrame = TangentFrame::At(kFrameAnchor),
                           .TerrainZoom = 15,
                           .CandidateGeneration = generation});
  };
  const auto finish = [&](uint64_t activeGeneration) {
    for (int attempt = 0; attempt < kMostPolls && queue.Busy(); ++attempt) {
      (void)tasks.AwaitCompletion(kPollWaitS);
      queue.Poll(tasks, activeGeneration);
    }
    CHECK(!queue.Busy(), "a bounded worker finishes within five seconds");
  };

  CHECK(submit(kFirstCandidate, fields), "the first candidate starts on the worker");
  finish(kFirstCandidate);
  auto built = queue.TakeCompleted();
  DescribeResult(built, source->SourceIdentity());
  CHECK(built && *built && built->value().Matches(kFirstCandidate, source->SourceIdentity()) &&
            built->value().Routes.size() == 1 &&
            built->value().Routes.front().Alignment->Closed() &&
            built->value().Routes.front().Alignment->Edges().size() == Circuit::EdgeCount &&
            built->value().Routes.front().Surface &&
            built->value().Routes.front().Surface->SurfaceGeometry.wellFormed() &&
            built->value().Earthworks.size() == built->value().Routes.front().Surface->Spans.size(),
        "the worker returns one native road surface and all source edges with provenance");

  CHECK(submit(kStaleCandidate, fields),
        "a later candidate starts after the first result is taken");
  finish(kCurrentCandidate);
  CHECK(!queue.TakeCompleted(), "a completed obsolete candidate cannot land");

  fields.pop_back();
  CHECK(submit(kMissingCandidate, std::move(fields)), "the missing-DEM candidate starts");
  finish(kMissingCandidate);
  auto missing = queue.TakeCompleted();
  CHECK(missing && !*missing &&
            missing->error().Code == RoadAlignmentBuildErrorCode::MissingTerrain &&
            missing->error().Tile == coverage->Tiles.back() &&
            missing->error().RouteId == "grand-prix",
        "missing sourced terrain fails with route and tile identity instead of a fallback");
  CHECK(queue.TryStart(tasks,
                       {.Source = source,
                        .Terrain = Generators::TerrainFieldSet(
                            std::vector<Generators::TerrainFieldSet::Entry>{}),
                        .RouteIndices = {0, 0},
                        .RenderFrame = TangentFrame::At(kFrameAnchor),
                        .TerrainZoom = 15,
                        .CandidateGeneration = kDuplicateCandidate}),
        "a malformed request reaches the bounded worker validator");
  finish(kDuplicateCandidate);
  auto duplicate = queue.TakeCompleted();
  CHECK(duplicate && !*duplicate &&
            duplicate->error().Code == RoadAlignmentBuildErrorCode::InvalidRequest,
        "the same source route cannot consume the candidate budget twice");
  return Report();
}
