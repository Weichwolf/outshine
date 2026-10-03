#include "BuildingMesh.h"
#include "Check.h"
#include "Digest.h"
#include "SurfacePreparation.h"
#include "OfflineTransport.h"
#include "Sink.h"
#include "StructureBuildQueue.h"
#include "StructureCell.h"
#include "TangentFrame.h"
#include "TileGeodesy.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

class SilentSink final : public outshine::Sink {
public:
  void Number(const char *, double, const char *) override {}

  void Claim(bool, const char *) override {}

  void Near(double, double, double, const char *, const char *) override {}

  void Say(const std::string &) override {}
};

struct TemporaryCache {
  std::filesystem::path Path =
      std::filesystem::temp_directory_path() /
      ("outshine-structure-cell-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

  ~TemporaryCache() {
    std::error_code error;
    std::filesystem::remove_all(Path, error);
  }
};

}

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  TemporaryCache cache;
  SilentSink sink;
  Data::OfflineTransport wire;
  Tasks sourceCompute(1);
  SurfacePreparation stack;
  const LongitudeLatitude eye{.LongitudeDeg = 8.5659, .LatitudeDeg = 49.3274};
  const std::array providers{Data::SourceProvider{.Kind = "terrain"}};
  CHECK(stack.Open({.Shipped = "src/assets", .Cache = cache.Path.string()},
                   providers,
                   eye,
                   wire,
                   sourceCompute,
                   sink,
                   nullptr,
                   1.0),
        "offline stack opens");
  if (!stack.Opened()) { return Report(); }
  const std::array<::outshine::Generators::Osm::OsmField::Declared, 1> buildings{{
      {.Layer = "buildings",
       .Key = "kind",
       .Value = "building",
       .HeightM = 12,
       .Area = true,
       .LatLon = {49.32739, 8.56589, 49.32739, 8.56591, 49.32741, 8.56591, 49.32741, 8.56589}},
  }};
  stack.Declares(buildings);
  CHECK(stack.AdvanceAt(eye, {.IngestTilesMost = 1, .VectorRing = 0}).has_value(),
        "declared building enters the vector snapshot");
  const ::outshine::Generators::Osm::OsmField *vectors = stack.Vectors();
  CHECK(vectors && vectors->Tiles().size() == 1 && !vectors->Rings().empty(),
        "one building tile is available");
  if (!vectors || vectors->Tiles().size() != 1 || vectors->Rings().empty()) { return Report(); }
  const auto original = vectors->Tiles().front();
  const auto bounds = TileBounds({.Zoom = original.Z,
                                  .X = static_cast<uint32_t>(original.X),
                                  .Y = static_cast<uint32_t>(original.Y)});
  std::array<::outshine::Generators::Osm::OsmField::Declared, 8> spread;
  for (size_t at = 0; at < spread.size(); ++at) {
    const double lon = bounds.MinLonDeg + (static_cast<double>(at) + 0.5) / 8.0 *
                                              (bounds.MaxLonDeg - bounds.MinLonDeg);
    const double lat = bounds.MaxLatDeg - 0.5 / 8.0 * (bounds.MaxLatDeg - bounds.MinLatDeg);
    constexpr double radius = 0.00001;
    spread[at] = {.Layer = "buildings",
                  .Key = "kind",
                  .Value = "building",
                  .HeightM = 12,
                  .Area = true,
                  .LatLon = {lat - radius,
                             lon - radius,
                             lat - radius,
                             lon + radius,
                             lat + radius,
                             lon + radius,
                             lat + radius,
                             lon - radius}};
  }
  stack.Declares(spread);
  CHECK(stack.AdvanceAt(eye, {.IngestTilesMost = 1, .VectorRing = 0}).has_value(),
        "eight independent buildings enter the source tile");
  const auto &tile = vectors->Tiles().front();
  auto field = std::make_shared<TerrainField>(3, 3);
  std::fill_n(field->Data(), 9, 100.0f);
  const Data::TileId demTile{.Zoom = 0, .X = 0, .Y = 0};
  field->AddSource({.Kind = Data::DataKind::Elevation,
                    .Tile = demTile,
                    .SourceId = "batch-dem",
                    .Revision = "original"});
  HeightField::Block block;
  CHECK(HeightField::SharesField(field, demTile, block), "shared analytic constant-height world");
  HeightField::Block near, remote;
  CHECK(HeightField::ResamplesSourcedAncestor(*field, demTile, {.Zoom = 1, .X = 1, .Y = 0}, near) &&
            HeightField::ResamplesSourcedAncestor(
                *field, demTile, {.Zoom = 1, .X = 0, .Y = 1}, remote),
        "one source includes both requested and unrelated terrain regions");
  auto pinned = HeightField::Of(1, {near, remote});
  ::outshine::Generators::Osm::BuildingField &prints = stack.Footprints();
  prints.AnchorAt(TangentFrame::At(eye).OriginEcef());
  prints.TilesSpan(1000);
  prints.SeenWith({.FocalPx = 720});
  ::outshine::Generators::Osm::BuildingField::Baked accepted{.OccupiedCells = 255};
  for (size_t at = 0; at < spread.size(); ++at) {
    const auto &ring = spread[at].LatLon;
    accepted.CellBounds[at] = {
        .MinLonDeg = ring[1], .MinLatDeg = ring[0], .MaxLonDeg = ring[3], .MaxLatDeg = ring[4]};
  }
  prints.PreparesAcceptances({.Tiles = 1});
  prints.Take(0);
  auto pending = prints.PrepareAcceptance(0,
                                          accepted,
                                          pinned->Sources(),
                                          true,
                                          tile.Source,
                                          {.HeightRasterDigest = pinned->RasterDigest(),
                                           .StreetDigest = kDigestBasis,
                                           .Projection = {.FocalPx = 720},
                                           .TileSpanM = 1000,
                                           .Eye = eye},
                                          1,
                                          pinned->CaptureRequest());
  prints.CommitAcceptance(std::move(pending), *vectors, accepted);
  const auto key = StructureBuildQueue::QualifiedSourceKey(prints, 0);
  CHECK(key.has_value(), "all eight cells have an accepted native source");
  if (!key) { return Report(); }
  Tasks pool(2);
  Generators::BuildingMesh mesher;
  StructureBuildQueue queue;
  queue.Opens(&pool, &mesher);
  size_t captures = 0;
  bool onlyRequiredTerrain = true;
  size_t frameCopies = 0;
  const std::array<SourcedTerrainFields::Entry, 1> fields{{{demTile, field}}};
  const StructureBuildQueue::HeightSource heights{
      .PinField =
          [&frameCopies](Data::TileId, HeightField::Block &) {
            ++frameCopies;
            return false;
          },
      .ResidentField = [&field, demTile](Data::TileId at) -> std::shared_ptr<const TerrainField> {
        return at == demTile ? field : nullptr;
      },
      .Revision = {.Value = 1},
      .CaptureFields =
          [&captures, &fields, &onlyRequiredTerrain](std::span<const TileSpot> requests,
                                                     size_t bytesMost) {
            ++captures;
            onlyRequiredTerrain &= requests.size() == 1 && requests.front().Zoom == 1 &&
                                   requests.front().X == 1 && requests.front().Y == 0;
            return SourcedTerrainFields::Capture(fields, requests, bytesMost);
          }};
  std::array<StructureBuildQueue::CellRequest, 8> requests;
  for (uint32_t at = 0; at < requests.size(); ++at) {
    requests[at] = {.Tile = 0,
                    .Cell = at + 1u,
                    .Detail = at % 2 == 0 ? LevelOfDetail::Fine : LevelOfDetail::Massed,
                    .SourceKey = *key};
  }
  auto oversized = heights;
  oversized.CaptureFields =
      [](std::span<const TileSpot>,
         size_t) -> std::expected<SourcedTerrainFields, SourcedTerrainFields::CaptureError> {
    return std::unexpected(SourcedTerrainFields::CaptureError::OverBudget);
  };
  CHECK(!queue.PostsCells(stack, prints, eye, oversized, requests) && queue.QueuedCells() == 0,
        "an impossible input budget returns an error without leaving a permanently deferred job");
  CHECK(queue.PostsCells(stack, prints, eye, heights, requests) == 8 && captures == 1 &&
            queue.QueuedCells() == 8 && frameCopies == 0 && onlyRequiredTerrain,
        "eight cells reserve only their terrain region and exclude the remote parent input");
  CHECK(queue.PostsCells(stack, prints, eye, heights, requests) == 0 && captures == 1,
        "queued demand cannot reserve a duplicate preparation");
  const auto revision = prints.Revision();
  uint64_t landed = 0;
  for (size_t attempt = 0; attempt < 400 && landed != 255; ++attempt) {
    auto result = queue.NextCellLanding(stack, prints, heights);
    CHECK(result.has_value(), "batched source preparation and bake complete without error");
    if (!result) { break; }
    if (*result) {
      const auto &one = **result;
      const auto cell = one.Baked->RequestedCell.value_or(0);
      CHECK(cell >= 1 && cell <= 8, "landing carries a requested cell");
      if (cell >= 1 && cell <= 8) {
        const uint64_t bit = uint64_t{1} << (cell - 1u);
        CHECK((landed & bit) == 0 && one.Baked->RequestedDetail == requests[cell - 1u].Detail &&
                  one.SourceKey == *key && one.Baked->OccupiedCells == bit,
              "each independent cell retains its exact detail and source identity");
        landed |= bit;
      }
      queue.CommitsCellLanding(one);
    } else {
      (void)queue.AwaitSlice(0.02);
    }
  }
  CHECK(landed == 255 && queue.QueuedCells() == 0 && captures == 1 && frameCopies == 0 &&
            prints.Revision() == revision,
        "all eight products land from one preparation without semantic reacceptance");
  (void)queue.NextCellLanding(stack, prints, heights);
  CHECK(queue.PostsCells(stack, prints, eye, heights, requests) == 8 && captures == 2,
        "completed demand releases its preparation rather than becoming a persistent cache");
  auto changed = heights;
  changed.Revision.Value = 2;
  for (size_t attempt = 0; attempt < 100 && queue.QueuedCells() != 0; ++attempt) {
    auto result = queue.NextCellLanding(stack, prints, changed);
    CHECK(result && !*result, "source revision replacement prevents old batch publication");
    (void)queue.AwaitSlice(0.02);
  }
  CHECK(queue.QueuedCells() == 0, "revoked demand leaves no queued cells");
  CHECK(queue.PostsCells(stack, prints, eye, heights, requests) == 8,
        "live detail demand exists before a ground replacement starts");
  bool heldLanding = false;
  for (size_t attempt = 0; attempt < 100 && !heldLanding; ++attempt) {
    const auto ready = queue.NextCellLanding(stack, prints, heights);
    heldLanding = ready && ready->has_value();
    if (!heldLanding) { (void)queue.AwaitSlice(0.02); }
  }
  CHECK(heldLanding && queue.QueuedCells() != 0,
        "completed detail work retains shared admission slots until a consumer retires it");
  auto candidate = prints.SnapshotAccepted();
  candidate.ResetDerived();
  candidate.BeginRefinement();
  auto candidateHeights = heights;
  candidateHeights.PinField = [&block](Data::TileId, HeightField::Block &into) {
    into = block;
    return true;
  };
  candidateHeights.Revision.Value = 2;
  bool sourceLanded = false;
  for (size_t attempt = 0; attempt < 100 && (!sourceLanded || queue.QueuedCells() != 0);
       ++attempt) {
    auto ready = queue.NextLandings(stack,
                                    candidate,
                                    eye,
                                    candidateHeights,
                                    1,
                                    StructureBuildQueue::HeightRequirement::FineOnly,
                                    std::nullopt,
                                    StructureBuildQueue::BuildPurpose::SourceGeometry);
    CHECK(ready.has_value(), "source replacement does not publish cancelled cell errors");
    if (ready && !ready->empty()) {
      queue.CommitsLandings(stack, candidate, *ready);
      sourceLanded = true;
    }
    (void)queue.Posts(stack,
                      candidate,
                      eye,
                      candidateHeights,
                      1,
                      StructureBuildQueue::HeightRequirement::FineOnly,
                      std::nullopt,
                      StructureBuildQueue::BuildPurpose::SourceGeometry);
    (void)queue.AwaitSlice(0.02);
  }
  CHECK(sourceLanded && queue.QueuedCells() == 0 && candidate.AcceptedTiles().size() == 1,
        "candidate source geometry progresses without a live-view cell consumer or larger queue");
  CHECK(prints.Revision() == revision && StructureBuildQueue::QualifiedSourceKey(prints, 0) == key,
        "retiring pending detail does not delete or change the published source snapshot");
  queue.Clear();
  return Report();
}
