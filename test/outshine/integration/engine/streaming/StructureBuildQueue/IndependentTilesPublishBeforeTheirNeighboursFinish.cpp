#include "test/harness/engine/NativeBuildingReplay.h"
#include "test/outshine/src/engine/streaming/StructureBuildQueue/BuildingQueueFixture.h"
#include "BuildingMesh.h"
#include "BuiltinSourceProviders.h"
#include "Check.h"
#include "JobAdmissionFixture.h"
#include "StructureBuildQueue.h"
#include "TerrainRevisionIndex.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr LongitudeLatitude kEye{.LongitudeDeg = 0.01, .LatitudeDeg = 0.01};
constexpr auto kPatience = std::chrono::seconds(10);

struct Fixture {
  BuildingQueueCache Directory;
  BuildingReplay::ReplaySourceTransport Wire{true};
  Tasks Sources{1}, Workers{2};
  Ground::SurfacePreparation Stack;
  StructureBuildQueue Queue;
  std::unique_ptr<Ground::TerrainRevisionIndex> Revisions;
  std::vector<std::pair<Data::TileId, std::shared_ptr<Ground::TerrainField>>> Heights;

  bool Open() {
    std::array<Data::SourceProvider, 2> providers{};
    providers[0].Kind = "terrain";
    providers[1].Kind = "vector";
    providers[1].Dataset = "fixture.independent-building-tiles";
    providers[1].Endpoint = "https://fixture.invalid/{z}/{x}/{y}.pbf";
    Data::ProviderRegistry registry;
    Generators::RegisterBuiltinSourceProviders(registry);
    if (!Stack.Open({.BuiltinData = "data",
                     .Sources = (Directory.Path / "sources").string(),
                     .Assets = (Directory.Path / "assets").string()},
                    providers,
                    registry,
                    kEye,
                    Wire,
                    Sources,
                    nullptr,
                    1.0)) {
      return false;
    }
    const auto deadline = std::chrono::steady_clock::now() + kPatience;
    do {
      if (!Stack.AdvanceAt(kEye, {.IngestTilesMost = 9, .VectorRing = 1})) { return false; }
      (void)Stack.AwaitProgress(0.001);
    } while ((!Stack.Ingested() || !Stack.Vectors() || !Stack.Vectors()->SettledWithin(1)) &&
             std::chrono::steady_clock::now() < deadline);
    if (!Stack.Ingested() || !Stack.Vectors() || Stack.Vectors()->Tiles().size() < 2) {
      return false;
    }
    auto revisions = Ground::TerrainRevisionIndex::Create(128);
    if (!revisions) { return false; }
    Revisions = std::move(*revisions);
    auto &prints = Stack.Footprints();
    prints.AnchorAt(TangentFrame::At(kEye).OriginEcef());
    prints.TilesSpan(1000);
    prints.SeenWith({.FocalPx = 720});
    Queue.Opens(&Workers,
                std::make_shared<Generators::BuildingMesh>(),
                JobAdmissions(),
                ContextAdmissions());
    return true;
  }

  bool Pin(Data::TileId tile, Ground::HeightField::Block &into) {
    const auto found =
        std::ranges::find_if(Heights, [tile](const auto &entry) { return entry.first == tile; });
    if (found != Heights.end()) {
      return Ground::HeightField::SharesField(found->second, tile, into);
    }
    const auto stamp = Revisions->IssueDeliveryStamp(tile);
    if (!stamp) { return false; }
    auto field = std::make_shared<Ground::TerrainField>(3, 3);
    std::fill_n(field->Data(), 9, 100.0f);
    field->AddSource({.Kind = Data::DataKind::Elevation,
                      .Tile = tile,
                      .SourceId = "analytic-plane",
                      .Revision = "one"});
    field->SetCertificate(Ground::TerrainCertificate::FromDelivery(tile, *stamp, 11));
    Heights.emplace_back(tile, field);
    return Ground::HeightField::SharesField(field, tile, into);
  }
};
}

int main() {
  Fixture fixture;
  const bool opened = fixture.Open();
  CHECK(opened, "neighbouring vector tiles contain actual buildings in separate native cells");
  if (!opened) { return Report(); }
  const StructureBuildQueue::HeightSource height{
      .Sample = {},
      .PinField = [&fixture](Data::TileId tile,
                             Ground::HeightField::Block &into) { return fixture.Pin(tile, into); },
      .Revision = {.Value = 11}};
  auto &prints = fixture.Stack.Footprints();
  std::set<uint32_t> partialTiles, completeTiles;
  bool independent = false;
  size_t posted = 0;
  const auto deadline = std::chrono::steady_clock::now() + kPatience;
  while (completeTiles.size() < 2 && std::chrono::steady_clock::now() < deadline) {
    if (posted < 2) {
      if (!fixture.Stack.AdvanceAt(kEye, {.IngestTilesMost = 9, .VectorRing = 1})) {
        return Report();
      }
      posted += fixture.Queue.Posts(fixture.Stack,
                                    prints,
                                    kEye,
                                    height,
                                    9,
                                    StructureBuildQueue::HeightRequirement::AllowFallback,
                                    LevelOfDetail::Fine);
    }
    if (posted < 2) {
      (void)fixture.Queue.AwaitSlice(0.001);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }
    auto ready = fixture.Queue.NextLandings(fixture.Stack,
                                            prints,
                                            kEye,
                                            height,
                                            2,
                                            StructureBuildQueue::HeightRequirement::AllowFallback,
                                            LevelOfDetail::Fine);
    CHECK(ready, "independent native tile products finish without geometry errors");
    if (!ready) { return Report(); }
    for (const auto &landing : *ready) {
      const auto &tile = *landing.Baked;
      CHECK(!tile.Built.WallRun.empty() && !tile.Built.RoofRun.empty() && tile.Prints.size() == 1 &&
                !tile.SelectedView,
            "each cell publishes the actual complete intrinsic facade and roof");
      if (landing.SourceComplete) {
        completeTiles.insert(landing.Tile);
      } else {
        partialTiles.insert(landing.Tile);
        independent |= partialTiles.size() >= 2 && completeTiles.empty();
      }
    }
    fixture.Queue.CommitsLandings(prints, *ready);
    if (!fixture.Queue.AwaitSlice(0.001)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
  CHECK(independent, "a neighbour becomes usable while the first tile still has missing cells");
  CHECK(
      completeTiles.size() == 2 &&
          std::ranges::all_of(completeTiles,
                              [&](uint32_t tile) { return prints.OfTile(tile).size() == 2; }),
      "both complete tile products preserve every source footprint after independent publication");
  return Report();
}
