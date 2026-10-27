#ifndef OUTSHINE_TEST_STRUCTUREBUILDQUEUE_BUILDINGQUEUEFIXTURE_H
#define OUTSHINE_TEST_STRUCTUREBUILDQUEUE_BUILDINGQUEUEFIXTURE_H

#include "BuildingMesh.h"
#include "Check.h"
#include "OfflineSourceTransport.h"
#include "BuiltinSourceProviders.h"
#include "Sink.h"
#include "StructureBuildQueue.h"
#include "StructureCell.h"
#include "SurfacePreparation.h"
#include "TangentFrame.h"
#include "TerrainRevisionIndex.h"
#include "TileGeodesy.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace outshine::Test {
class BuildingQueueSink final : public Sink {
public:
  void Number(const char * /*name*/, double /*value*/, const char * /*unit*/) override {}

  void Claim(bool /*passed*/, const char * /*claim*/) override {}

  void Near(double /*got*/,
            double /*want*/,
            double /*within*/,
            const char * /*unit*/,
            const char * /*why*/) override {}

  void Say(const std::string & /*line*/) override {}
};

struct BuildingQueueCache {
  std::filesystem::path Path =
      std::filesystem::temp_directory_path() /
      ("outshine-structure-cell-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

  ~BuildingQueueCache() {
    std::error_code error;
    std::filesystem::remove_all(Path, error);
  }
};

struct BuildingQueueFixture {
  BuildingQueueCache Cache;
  BuildingQueueSink Sink;
  Data::OfflineSourceTransport Wire;
  Tasks SourceCompute{1};
  Ground::SurfacePreparation Stack;
  const LongitudeLatitude Eye{.LongitudeDeg = 8.5659, .LatitudeDeg = 49.3274};
  const std::array<Generators::Osm::OsmField::Declared, 1> Buildings{{
      {.Layer = "buildings",
       .Key = "kind",
       .Value = "building",
       .HeightM = 12,
       .Area = true,
       .LatLon = {49.32739, 8.56589, 49.32739, 8.56591, 49.32741, 8.56591, 49.32741, 8.56589}},
  }};
  Data::TileId DemTile{};
  std::unique_ptr<Ground::TerrainRevisionIndex> Revisions;
  std::shared_ptr<Ground::TerrainField> Field;
  size_t Copies = 0;
  bool AllowCopy = true;
  StructureBuildQueue::HeightSource Source{
      .Sample = {},
      .PinField =
          [this](Data::TileId at, Ground::HeightField::Block &into) {
            ++Copies;
            return AllowCopy && Ground::HeightField::SharesField(Field, at, into);
          },
      .Revision = {.Value = 11}};
  Tasks Pool{1};
  Generators::BuildingMesh Mesher;
  StructureBuildQueue Queue;

  [[nodiscard]] bool Open() {
    Data::SourceProvider terrain;
    terrain.Kind = "terrain";
    const std::array providers{terrain};
    Data::ProviderRegistry registry;
    Generators::RegisterBuiltinSourceProviders(registry);
    CHECK(Stack.Open({.BuiltinData = "data", .Sources = Cache.Path.string(), .Assets = {}},
                     providers,
                     registry,
                     Eye,
                     Wire,
                     SourceCompute,
                     Sink,
                     nullptr,
                     1.0),
          "offline stack opens");
    if (!Stack.Opened()) { return false; }
    CHECK(Stack.Declares(Buildings), "building declarations are accepted");
    CHECK(Stack.AdvanceAt(Eye, {.IngestTilesMost = 1, .VectorRing = 0}).has_value(),
          "declared building enters the vector snapshot");
    const auto *vectors = Stack.Vectors();
    CHECK(vectors != nullptr && vectors->Tiles().size() == 1 && !vectors->Rings().empty(),
          "one building tile is available");
    if (vectors == nullptr || vectors->Tiles().size() != 1 || vectors->Rings().empty()) {
      return false;
    }
    const auto &tile = vectors->Tiles().front();
    const auto &ring = vectors->Rings().front();
    const auto cell = Generators::StructureCellOf(
        Ground::TileBounds({.Zoom = tile.Z,
                            .X = static_cast<uint32_t>(tile.X),
                            .Y = static_cast<uint32_t>(tile.Y)}),
        std::span(vectors->Points().data() + size_t{2} * ring.First, size_t{2} * ring.Count));
    CHECK(cell && cell->Index != 0, "declared footprint has a stable spatial cell");
    if (!cell) { return false; }
    auto &prints = Stack.Footprints();
    prints.AnchorAt(TangentFrame::At(Eye).OriginEcef());
    prints.TilesSpan(1000);
    prints.SeenWith({.FocalPx = 720});
    Queue.Opens(&Pool, &Mesher);
    return PrepareTerrain();
  }

  [[nodiscard]] bool PrepareTerrain() {
    const int zoom = Stack.FinestZoomOf(Data::DataKind::Elevation);
    const auto spot = Ground::HeightField::SpotOf(Eye, zoom);
    DemTile = {
        .Zoom = zoom, .X = static_cast<uint32_t>(spot.X), .Y = static_cast<uint32_t>(spot.Y)};
    auto revisions = Ground::TerrainRevisionIndex::Create(32);
    CHECK(revisions.has_value(), "revision index opens");
    if (!revisions) { return false; }
    Revisions = std::move(*revisions);
    const auto stamp = Revisions->IssueDeliveryStamp(DemTile);
    CHECK(stamp.has_value(), "initial DEM delivery exists");
    if (!stamp) { return false; }
    Field = std::make_shared<Ground::TerrainField>(3, 3);
    std::fill_n(Field->Data(), 9, 100.0f);
    Field->AddSource({.Kind = Data::DataKind::Elevation,
                      .Tile = DemTile,
                      .SourceId = "test-dem",
                      .Revision = "one"});
    Field->SetCertificate(Ground::TerrainCertificate::FromDelivery(DemTile, *stamp, 11));
    return true;
  }

  [[nodiscard]] size_t Post() {
    return Queue.Posts(Stack,
                       Stack.Footprints(),
                       Eye,
                       Source,
                       1,
                       StructureBuildQueue::HeightRequirement::AllowFallback);
  }

  [[nodiscard]] size_t Refine() {
    return Queue.Posts(Stack,
                       Stack.Footprints(),
                       Eye,
                       Source,
                       1,
                       StructureBuildQueue::HeightRequirement::FineOnly,
                       std::nullopt,
                       StructureBuildQueue::BuildPurpose::SourceGeometry);
  }

  [[nodiscard]] std::vector<StructureBuildQueue::Landing> Finish(bool refinement = false) {
    std::vector<StructureBuildQueue::Landing> landings;
    for (int attempt = 0; attempt < 100 && Queue.Queued() != 0; ++attempt) {
      auto ready =
          Queue.NextLandings(Stack,
                             Stack.Footprints(),
                             Eye,
                             Source,
                             1,
                             refinement ? StructureBuildQueue::HeightRequirement::FineOnly
                                        : StructureBuildQueue::HeightRequirement::AllowFallback,
                             std::nullopt,
                             refinement ? StructureBuildQueue::BuildPurpose::SourceGeometry
                                        : StructureBuildQueue::BuildPurpose::ViewDetail);
      CHECK(ready.has_value(), "whole-tile build completes without a mesher error");
      if (!ready) { break; }
      if (!ready->empty()) {
        landings = std::move(*ready);
        break;
      }
      (void)Queue.AwaitSlice(0.02);
    }
    return landings;
  }
};
}
#endif
