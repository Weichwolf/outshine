#include "BuildingMesh.h"
#include "Check.h"
#include "Digest.h"
#include "SurfacePreparation.h"
#include "OfflineTransport.h"
#include "Sink.h"
#include "StructureBuildQueue.h"
#include "StructureCell.h"
#include "TerrainRevisionIndex.h"
#include <algorithm>
#include "TangentFrame.h"
#include "TileGeodesy.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
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
  const auto &tile = vectors->Tiles().front();
  const auto &ring = vectors->Rings().front();
  const auto cell = Generators::StructureCellOf(
      TileBounds(
          {.Zoom = tile.Z, .X = static_cast<uint32_t>(tile.X), .Y = static_cast<uint32_t>(tile.Y)}),
      std::span(vectors->Points().data() + 2u * ring.First, 2u * ring.Count));
  CHECK(cell && cell->Index != 0, "declared footprint has a stable spatial cell");
  if (!cell) { return Report(); }
  ::outshine::Generators::Osm::BuildingField &prints = stack.Footprints();
  prints.AnchorAt(TangentFrame::At(eye).OriginEcef());
  prints.TilesSpan(1000);
  prints.SeenWith({.FocalPx = 720});
  const int zoom = stack.FinestZoomOf(Data::DataKind::Elevation);
  const auto spot = HeightField::SpotOf(eye, zoom);
  const Data::TileId demTile{
      .Zoom = zoom, .X = static_cast<uint32_t>(spot.X), .Y = static_cast<uint32_t>(spot.Y)};
  auto revisions = TerrainRevisionIndex::Create(32);
  CHECK(revisions.has_value(), "revision index opens");
  if (!revisions) { return Report(); }
  const auto stamp = (**revisions).IssueDeliveryStamp(demTile);
  CHECK(stamp.has_value(), "initial DEM delivery exists");
  if (!stamp) { return Report(); }
  auto field = std::make_shared<TerrainField>(3, 3);
  std::fill_n(field->Data(), 9, 100.0f);
  field->AddSource({.Kind = Data::DataKind::Elevation,
                    .Tile = demTile,
                    .SourceId = "test-dem",
                    .Revision = "one"});
  field->SetCertificate(TerrainCertificate::FromDelivery(demTile, *stamp, 11));
  size_t copies = 0;
  bool allowCopy = true;
  StructureBuildQueue::HeightSource source{
      .Sample = {},
      .PinField =
          [&field, &copies, &allowCopy](Data::TileId at, HeightField::Block &into) {
            ++copies;
            return allowCopy && HeightField::SharesField(field, at, into);
          },
      .ResidentField = {},
      .Revision = {.Value = 11}};
  Tasks pool(1);
  Generators::BuildingMesh mesher;
  StructureBuildQueue queue;
  queue.Opens(&pool, &mesher);
  const auto post = [&] {
    return queue.Posts(
        stack, prints, eye, source, 1, StructureBuildQueue::HeightRequirement::AllowFallback);
  };
  const auto finish = [&](bool refinement = false) {
    std::vector<StructureBuildQueue::Landing> landings;
    for (int attempt = 0; attempt < 100 && queue.Queued() != 0; ++attempt) {
      auto ready =
          queue.NextLandings(stack,
                             prints,
                             eye,
                             source,
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
      (void)queue.AwaitSlice(0.02);
    }
    return landings;
  };
  CHECK(post() == 1, "initial qualified tile is posted");
  const size_t beforeLanding = copies;
  allowCopy = false;
  auto ready = finish();
  CHECK(ready.size() == 1 && ready.front().Footprints.has_value(),
        "valid certificate lands even when input raster access is unavailable");
  CHECK(copies == beforeLanding, "certified whole-tile landing never resolves heights again");
  if (ready.empty()) { return Report(); }
  const auto sourceProduct =
      [&](LongitudeLatitude postedEye,
          double focalPx,
          std::optional<LevelOfDetail> detail =
              std::nullopt) -> std::optional<std::pair<uint64_t, Generators::BakedTile>> {
    queue.Clear();
    prints.Release(0);
    prints.BeginRefinement();
    prints.SeenWith({.FocalPx = focalPx});
    allowCopy = true;
    CHECK(queue.Posts(stack,
                      prints,
                      postedEye,
                      source,
                      1,
                      StructureBuildQueue::HeightRequirement::FineOnly,
                      detail,
                      StructureBuildQueue::BuildPurpose::SourceGeometry) == 1,
          "source geometry admits the declared source independently of the camera");
    const auto landedEye = LongitudeLatitude{.LongitudeDeg = postedEye.LongitudeDeg + 1.0,
                                             .LatitudeDeg = postedEye.LatitudeDeg};
    prints.SeenWith({.FocalPx = focalPx * 2.0});
    for (int attempt = 0; attempt < 100 && queue.Queued() != 0; ++attempt) {
      auto landed = queue.NextLandings(stack,
                                       prints,
                                       landedEye,
                                       source,
                                       1,
                                       StructureBuildQueue::HeightRequirement::FineOnly,
                                       detail,
                                       StructureBuildQueue::BuildPurpose::SourceGeometry);
      CHECK(landed.has_value(), "camera changes do not invalidate source geometry completion");
      if (!landed) { return std::nullopt; }
      if (!landed->empty()) { return std::pair{landed->front().SourceKey, *landed->front().Baked}; }
      (void)queue.AwaitSlice(0.02);
    }
    return std::nullopt;
  };
  const auto nearSource = sourceProduct(eye, 720.0);
  CHECK(nearSource.has_value(), "near source request lands");
  if (!nearSource) { return Report(); }
  const auto farSource = sourceProduct({.LongitudeDeg = 9.5, .LatitudeDeg = 49.3274}, 10.0);
  CHECK(nearSource && farSource, "near and remote source requests both land after camera changes");
  if (!farSource) { return Report(); }
  CHECK(!nearSource->second.RequestedDetail && !farSource->second.RequestedDetail &&
            nearSource->second.FootprintDetails == std::vector{LevelOfDetail::Fine} &&
            farSource->second.FootprintDetails == std::vector{LevelOfDetail::Massed},
        "implicit source products retain nearby detail and aggregate remote buildings");
  CHECK(nearSource->first != 0 && nearSource->first == farSource->first &&
            !nearSource->second.Built.WallRun.empty() && !farSource->second.Built.WallRun.empty() &&
            !farSource->second.Built.RoofRun.empty(),
        "view-dependent fallback keeps source identity and visible remote geometry");
  const auto nearShell = sourceProduct(eye, 720.0, LevelOfDetail::Shell);
  const auto farShell =
      sourceProduct({.LongitudeDeg = 9.5, .LatitudeDeg = 49.3274}, 10.0, LevelOfDetail::Shell);
  CHECK(nearShell && farShell && nearShell->second.Digest == farShell->second.Digest &&
            nearShell->second.RequestedDetail == LevelOfDetail::Shell &&
            farShell->second.RequestedDetail == LevelOfDetail::Shell,
        "explicit LOD products remain independent of camera position and focal length");
  const auto fineSource = sourceProduct(eye, 720.0, LevelOfDetail::Fine);
  CHECK(fineSource && fineSource->second.RequestedDetail == LevelOfDetail::Fine &&
            fineSource->second.FootprintDetails == std::vector{LevelOfDetail::Fine},
        "an explicit Fine source request is preserved");
  if (!fineSource) { return Report(); }
  prints.SeenWith({.FocalPx = 720.0});
  queue.Clear();
  prints.Release(0);
  allowCopy = true;
  CHECK(post() == 1, "tile can be posted again after owned reservation release");
  ++source.Revision.Value;
  ready = finish();
  CHECK(ready.empty() && queue.Queued() == 0 && queue.Discarded() != 0,
        "late whole-tile geometry with revoked DEM never becomes a landing");
  CHECK(post() == 0 && queue.Queued() == 0, "stale instrumented input cannot reserve new work");
  field->SetCertificate(TerrainCertificate::FromDelivery(demTile, std::nullopt, 11));
  source.Revision.Value = 12;
  CHECK(post() == 0, "changed terrain scope rejects a shaped snapshot with no delivery stamps");
  field->SetCertificate(TerrainCertificate::FromDelivery(demTile, std::nullopt, 12));
  CHECK(post() == 1, "current shaped scope retries the unreserved tile");
  source.Revision.Value = 13;
  ready = finish();
  CHECK(ready.empty() && queue.Queued() == 0,
        "old shaped completion cannot land from a frozen producer after scope change");
  TerrainCertificate mixed = TerrainCertificate::FromDelivery(demTile, std::nullopt, 12);
  mixed.Merge(TerrainCertificate::FromDelivery(demTile, std::nullopt, 13));
  CHECK(!mixed.ScopeCurrent(12) && !mixed.ScopeCurrent(13) && !mixed.ScopeCurrent(0),
        "conflicting known scopes cannot validate even without delivery stamps");
  source.Revision.Value = 11;
  field->SetCertificate(
      TerrainCertificate::FromDelivery(demTile, (**revisions).CurrentStamp(demTile), 11));
  CHECK(post() == 1, "re-certified delivery retries without a leaked reservation");
  CHECK((**revisions).IssueDeliveryStamp(demTile).has_value(),
        "unchanged raster receives another delivery before activation");
  field->SetCertificate(
      TerrainCertificate::FromDelivery(demTile, (**revisions).CurrentStamp(demTile), 11));
  ready = finish();
  CHECK(ready.size() == 1, "newly certified input completes normally");
  if (!ready.empty()) { queue.CommitsLandings(prints, ready); }
  CHECK(queue.Queued() == 0 && prints.InputOfTile(0) != nullptr,
        "only validated completion commits accepted footprint metadata");
  const auto *accepted = prints.InputOfTile(0);
  CHECK(accepted && accepted->HeightRevision == source.Revision.Value,
        "accepted products retain the exact input revision of their bake");
  CHECK(accepted && accepted->Heights.Zoom == zoom && !accepted->Heights.Fallback &&
            !accepted->Heights.Tiles.empty() && accepted->Heights.Tiles.front().Zoom == spot.Zoom &&
            accepted->Heights.Tiles.front().X == spot.X &&
            accepted->Heights.Tiles.front().Y == spot.Y,
        "whole-tile landing retains captured DEM requests after releasing its bake task");
  const auto refine = [&] {
    return queue.Posts(stack,
                       prints,
                       eye,
                       source,
                       1,
                       StructureBuildQueue::HeightRequirement::FineOnly,
                       std::nullopt,
                       StructureBuildQueue::BuildPurpose::SourceGeometry);
  };
  const auto takenBefore = prints.IngestedTiles();
  prints.BeginRefinement();
  field = std::make_shared<TerrainField>(*field);
  std::fill_n(field->Data(), 9, 110.0f);
  source.Revision.Value = 14;
  CHECK(refine() == 0 && queue.Queued() == 0 && prints.RefinementRemaining() == 1,
        "refused replacement admission preserves the refinement cursor");
  field->SetCertificate(
      TerrainCertificate::FromDelivery(demTile, (**revisions).CurrentStamp(demTile), 14));
  CHECK(refine() == 1 && prints.RefinementComplete(),
        "changed terrain posts one replacement and advances the refinement cursor");
  source.Revision.Value = 15;
  field = std::make_shared<TerrainField>(*field);
  std::fill_n(field->Data(), 9, 120.0f);
  field->SetCertificate(
      TerrainCertificate::FromDelivery(demTile, (**revisions).CurrentStamp(demTile), 15));
  ready = finish(true);
  CHECK(ready.empty() && queue.Queued() == 0 && prints.RefinementRemaining() == 1,
        "discarded owned replacement returns its tile to the refinement cursor");
  CHECK(refine() == 1, "the replacement retries against the new terrain delivery");
  ready = finish(true);
  CHECK(ready.size() == 1, "the retried source replacement becomes a validated landing");
  if (!ready.empty()) { queue.CommitsLandings(prints, ready); }
  CHECK(prints.RefinementComplete() && prints.IngestedTiles() == takenBefore,
        "replacement retry completes without a second whole-tile reservation");
  const auto acceptedKey = StructureBuildQueue::QualifiedSourceKey(prints, 0);
  CHECK(acceptedKey.has_value(), "validated tile has a qualified source key");
  source.Revision.Value = 12;
  if (acceptedKey) {
    CHECK(!StructureBuildQueue::ValidateResidentCellSource(stack, prints, source, 0, *acceptedKey),
          "readiness rejects frozen inputs from the previous terrain scope");
    CHECK(!queue.PostsCell(stack,
                           prints,
                           eye,
                           source,
                           {.Tile = 0,
                            .Cell = cell->Index,
                            .Detail = LevelOfDetail::Massed,
                            .SourceKey = *acceptedKey}),
          "cell posting rejects frozen inputs from the previous revision");
  }
  source.Revision.Value = 15;
  prints.ResetDerived();
  CHECK(post() == 1, "a fresh reservation is captured by its original field");
  ::outshine::Generators::Osm::BuildingField successor = prints.SnapshotAccepted();
  CHECK(successor.ReservationOwner() != prints.ReservationOwner() && successor.IngestedTiles() == 0,
        "successor snapshot drops pending reservations and gets its own owner");
  successor.Take(0);
  bool foreignLanding = false;
  for (int attempt = 0; attempt < 100 && queue.Queued() != 0; ++attempt) {
    const auto landing = queue.NextLandings(
        stack, successor, eye, source, 1, StructureBuildQueue::HeightRequirement::AllowFallback);
    CHECK(landing.has_value(), "foreign owner rejection preserves the bake error contract");
    foreignLanding |= landing && !landing->empty();
    (void)queue.AwaitSlice(0.02);
  }
  CHECK(!foreignLanding && queue.Queued() == 0 && successor.IngestedTiles() == 1,
        "old jobs neither land into nor release another owner's same-tile reservation");
  successor.Release(0);
  CHECK(successor.IngestedTiles() == 0,
        "only the successor owner releases its deliberately colliding reservation");
  prints.ResetDerived();
  CHECK(post() == 1, "current field reserves a tile before a producer revision change");
  const auto reservationOwner = prints.ReservationOwner();
  ++source.Revision.Value;
  ready = finish();
  CHECK(ready.empty() && queue.Queued() == 0 && prints.IngestedTiles() == 0 &&
            prints.ReservationOwner() == reservationOwner,
        "producer revision changes release the old job's reservation in the same domain");
  field->SetCertificate(
      TerrainCertificate::FromDelivery(demTile, std::nullopt, source.Revision.Value));
  CHECK(post() == 1, "new producer revision can reserve the released tile again");
  ready = finish();
  CHECK(ready.size() == 1, "new producer revision completes without an orphaned reservation");
  if (!ready.empty()) { queue.CommitsLandings(prints, ready); }
  auto lineOnly = buildings;
  lineOnly.front().Area = false;
  stack.Declares(lineOnly);
  CHECK(stack.AdvanceAt(eye, {.IngestTilesMost = 1, .VectorRing = 0}).has_value(),
        "line-only vector input enters the source snapshot");
  size_t terrainCalls = 0;
  source.PinField = [&terrainCalls](Data::TileId, HeightField::Block &) {
    ++terrainCalls;
    return false;
  };
  source.Sample = [&terrainCalls](LongitudeLatitude) -> std::optional<double> {
    ++terrainCalls;
    return std::nullopt;
  };
  prints.BeginRefinement();
  CHECK(refine() == 1, "a vector tile without building polygons needs no DEM reservation");
  ready = finish(true);
  CHECK(ready.size() == 1 && ready.front().Baked->OccupiedCells == 0 && terrainCalls == 0,
        "empty building geometry lands without invoking terrain resolvers");
  if (!ready.empty()) { queue.CommitsLandings(prints, ready); }
  CHECK(StructureBuildQueue::QualifiedSources(stack, prints),
        "proven building absence completes the qualified source snapshot");
  stack.Declares(buildings);
  CHECK(stack.AdvanceAt(eye, {.IngestTilesMost = 1, .VectorRing = 0}).has_value(),
        "a real building polygon replaces the line-only input");
  prints.BeginRefinement();
  CHECK(refine() == 0 && !StructureBuildQueue::QualifiedSources(stack, prints) && terrainCalls > 0,
        "missing DEM for a real polygon still defers rather than certifying absence");
  return Report();
}
