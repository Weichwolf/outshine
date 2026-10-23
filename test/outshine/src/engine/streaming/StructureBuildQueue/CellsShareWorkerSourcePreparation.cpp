#include "BuildingMesh.h"
#include "Check.h"
#include "DeclaredBuildingFixture.h"
#include "Digest.h"
#include "SurfacePreparation.h"
#include "OfflineTransport.h"
#include "Sink.h"
#include "StructureBuildQueue.h"
#include "StructureCell.h"
#include "TangentFrame.h"
#include "TileGeodesy.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {
constexpr auto kWaitSliceS = 0.02;
constexpr auto kAttemptBudget = 100;
constexpr auto kTileSpanM = 1000;
constexpr auto kOccupiedCells = 255;
constexpr auto kBatchAttemptBudget = 400;
constexpr auto kFocalPx = 720;

class SilentSink final : public outshine::Sink {
public:
  void Number(const char * /*what*/, double /*value*/, const char * /*unit*/) override {}

  void Claim(bool /*held*/, const char * /*why*/) override {}

  void Near(double /*got*/,
            double /*want*/,
            double /*within*/,
            const char * /*unit*/,
            const char * /*why*/) override {}

  void Say(const std::string & /*line*/) override {}
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

auto SpreadBuildings(const outshine::Ground::SurfacePreparation &stack) {
  using namespace outshine;
  const auto original = stack.Vectors()->Tiles().front();
  const auto bounds = outshine::Ground::TileBounds({.Zoom = original.Z,
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
  return spread;
}

void CheckCandidateReplacement(outshine::Ground::SurfacePreparation &stack,
                               outshine::Generators::Osm::BuildingField &prints,
                               outshine::StructureBuildQueue &queue,
                               const outshine::StructureBuildQueue::HeightSource &heights,
                               outshine::LongitudeLatitude eye,
                               const outshine::Ground::HeightField::Block &block,
                               uint64_t revision,
                               const std::optional<uint64_t> &key) {
  using namespace outshine;
  using namespace outshine::Test;
  auto candidate = prints.SnapshotAccepted();
  candidate.ResetDerived();
  candidate.BeginRefinement();
  auto candidateHeights = heights;
  candidateHeights.PinField = [&block](Data::TileId, outshine::Ground::HeightField::Block &into) {
    into = block;
    return true;
  };
  candidateHeights.Revision.Value = 2;
  bool sourceLanded = false;
  for (size_t attempt = 0; attempt < kAttemptBudget && (!sourceLanded || queue.QueuedCells() != 0);
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
      queue.CommitsLandings(candidate, *ready);
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
    (void)queue.AwaitSlice(kWaitSliceS);
  }
  CHECK(sourceLanded && queue.QueuedCells() == 0 && candidate.AcceptedTiles().size() == 1,
        "candidate source geometry progresses without a live-view cell consumer or larger queue");
  CHECK(prints.Revision() == revision && StructureBuildQueue::QualifiedSourceKey(prints, 0) == key,
        "retiring pending detail does not delete or change the published source snapshot");
}

uint64_t LandCells(outshine::Ground::SurfacePreparation &stack,
                   outshine::Generators::Osm::BuildingField &prints,
                   outshine::StructureBuildQueue &queue,
                   const outshine::StructureBuildQueue::HeightSource &heights,
                   const std::array<outshine::StructureBuildQueue::CellRequest, 8> &requests,
                   uint64_t key) {
  using namespace outshine::Test;
  uint64_t landed = 0;
  for (size_t attempt = 0; attempt < kBatchAttemptBudget && landed != kOccupiedCells; ++attempt) {
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
                  one.SourceKey == key && one.Baked->OccupiedCells == bit,
              "each independent cell retains its exact detail and source identity");
        landed |= bit;
      }
      queue.CommitsCellLanding(one);
    } else {
      (void)queue.AwaitSlice(kWaitSliceS);
    }
  }
  return landed;
}

void CheckRevokedDemand(outshine::Ground::SurfacePreparation &stack,
                        outshine::Generators::Osm::BuildingField &prints,
                        outshine::StructureBuildQueue &queue,
                        const outshine::StructureBuildQueue::HeightSource &heights,
                        outshine::LongitudeLatitude eye,
                        const std::array<outshine::StructureBuildQueue::CellRequest, 8> &requests) {
  using namespace outshine::Test;
  auto changed = heights;
  changed.Revision.Value = 2;
  for (size_t attempt = 0; attempt < kAttemptBudget && queue.QueuedCells() != 0; ++attempt) {
    auto result = queue.NextCellLanding(stack, prints, changed);
    CHECK(result && !*result, "source revision replacement prevents old batch publication");
    (void)queue.AwaitSlice(kWaitSliceS);
  }
  CHECK(queue.QueuedCells() == 0, "revoked demand leaves no queued cells");
  CHECK(queue.PostsCells(stack, prints, eye, heights, requests) == 8,
        "live detail demand exists before a ground replacement starts");
  bool heldLanding = false;
  for (size_t attempt = 0; attempt < kAttemptBudget && !heldLanding; ++attempt) {
    const auto ready = queue.NextCellLanding(stack, prints, heights);
    heldLanding = ready && ready->has_value();
    if (!heldLanding) { (void)queue.AwaitSlice(kWaitSliceS); }
  }
  CHECK(heldLanding && queue.QueuedCells() != 0,
        "completed detail work retains shared admission slots until a consumer retires it");
}

void AcceptSources(outshine::Generators::Osm::BuildingField &prints,
                   outshine::LongitudeLatitude eye,
                   const std::array<outshine::Generators::Osm::OsmField::Declared, 8> &spread,
                   const std::shared_ptr<const outshine::Ground::HeightField> &pinned,
                   const outshine::Data::TileSourceIdentity &source) {
  using namespace outshine;
  prints.AnchorAt(TangentFrame::At(eye).OriginEcef());
  prints.TilesSpan(kTileSpanM);
  prints.SeenWith({.FocalPx = kFocalPx});
  ::outshine::Generators::Osm::BuildingField::Baked accepted{.Coordinates = {},
                                                             .Prints = {},
                                                             .SeatSpreadM = {},
                                                             .AcrossM = {},
                                                             .OccupiedCells = kOccupiedCells};
  for (size_t at = 0; at < spread.size(); ++at) {
    const auto &ring = spread[at].LatLon;
    accepted.CellBounds[at] = {
        .MinLonDeg = ring[1], .MinLatDeg = ring[0], .MaxLonDeg = ring[3], .MaxLatDeg = ring[4]};
  }
  prints.PreparesAcceptances(1);
  prints.Take(0);
  auto pending = prints.PrepareAcceptance(0,
                                          accepted,
                                          pinned->Sources(),
                                          true,
                                          source,
                                          {.HeightRasterDigest = pinned->RasterDigest(),
                                           .StreetDigest = kDigestBasis,
                                           .Projection = {.FocalPx = kFocalPx},
                                           .TileSpanM = kTileSpanM,
                                           .Eye = eye,
                                           .EyeEcef = {}},
                                          1,
                                          pinned->CaptureRequest());
  prints.CommitAcceptance(std::move(pending), accepted);
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  const TemporaryCache cache;
  SilentSink sink;
  Data::OfflineTransport wire;
  Tasks sourceCompute(1);
  SurfacePreparation stack;
  const LongitudeLatitude eye{.LongitudeDeg = 8.5659, .LatitudeDeg = 49.3274};
  if (!OpenDeclaredBuilding(stack, cache.Path.string(), eye, wire, sourceCompute, sink)) {
    return Report();
  }
  const auto *vectors = stack.Vectors();
  const auto spread = SpreadBuildings(stack);
  CHECK(stack.Declares(spread), "declared source snapshot is accepted");
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
  HeightField::Block near;
  HeightField::Block remote;
  CHECK(HeightField::ResamplesSourcedAncestor(*field, demTile, {.Zoom = 1, .X = 1, .Y = 0}, near) &&
            HeightField::ResamplesSourcedAncestor(
                *field, demTile, {.Zoom = 1, .X = 0, .Y = 1}, remote),
        "one source includes both requested and unrelated terrain regions");
  auto pinned = HeightField::Of(1, {near, remote});
  ::outshine::Generators::Osm::BuildingField &prints = stack.Footprints();
  AcceptSources(prints, eye, spread, pinned, tile.Source);
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
      .Sample = {},
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
  const auto landed = LandCells(stack, prints, queue, heights, requests, *key);
  CHECK(landed == kOccupiedCells && queue.QueuedCells() == 0 && captures == 1 && frameCopies == 0 &&
            prints.Revision() == revision,
        "all eight products land from one preparation without semantic reacceptance");
  const auto retired = queue.NextCellLanding(stack, prints, heights);
  CHECK(retired.has_value(), "completed batch retires without a cell error");
  CHECK(queue.PostsCells(stack, prints, eye, heights, requests) == 8 && captures == 2,
        "completed demand releases its preparation rather than becoming a persistent cache");
  CheckRevokedDemand(stack, prints, queue, heights, eye, requests);
  CheckCandidateReplacement(stack, prints, queue, heights, eye, block, revision, key);
  queue.Clear();
  return Report();
}
