#include "BuildingQueueFixture.h"
#include "Check.h"
#include "Digest.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Ground;
using namespace outshine::Test;

std::optional<std::pair<uint64_t, Generators::BakedTile>>
BuildSource(BuildingQueueFixture &fixture,
            LongitudeLatitude postedEye,
            double focalPx,
            std::optional<LevelOfDetail> detail = std::nullopt) {
  auto &prints = fixture.Stack.Footprints();
  fixture.Queue.Clear();
  prints.Release(0);
  prints.BeginRefinement();
  prints.SeenWith({.FocalPx = focalPx});
  fixture.AllowCopy = true;
  CHECK(fixture.Queue.Posts(fixture.Stack,
                            prints,
                            postedEye,
                            fixture.Source,
                            1,
                            StructureBuildQueue::HeightRequirement::FineOnly,
                            detail,
                            StructureBuildQueue::BuildPurpose::SourceGeometry) == 1,
        "source geometry admits the declared source independently of the camera");
  const auto landedEye = LongitudeLatitude{.LongitudeDeg = postedEye.LongitudeDeg + 1.0,
                                           .LatitudeDeg = postedEye.LatitudeDeg};
  prints.SeenWith({.FocalPx = focalPx * 2.0});
  for (int attempt = 0; attempt < 100 && fixture.Queue.Queued() != 0; ++attempt) {
    auto landed = fixture.Queue.NextLandings(fixture.Stack,
                                             prints,
                                             landedEye,
                                             fixture.Source,
                                             1,
                                             StructureBuildQueue::HeightRequirement::FineOnly,
                                             detail,
                                             StructureBuildQueue::BuildPurpose::SourceGeometry);
    CHECK(landed.has_value(), "camera changes do not invalidate source geometry completion");
    if (!landed) { return std::nullopt; }
    if (!landed->empty()) { return std::pair{landed->front().SourceKey, *landed->front().Baked}; }
    (void)fixture.Queue.AwaitSlice(0.02);
  }
  return std::nullopt;
}

bool CheckCameraIndependentSources(BuildingQueueFixture &fixture) {
  CHECK(fixture.Post() == 1, "initial qualified tile is posted");
  const size_t beforeLanding = fixture.Copies;
  fixture.AllowCopy = false;
  auto ready = fixture.Finish();
  CHECK(ready.size() == 1 && ready.front().Footprints.has_value(),
        "valid certificate lands even when input raster access is unavailable");
  CHECK(fixture.Copies == beforeLanding,
        "certified whole-tile landing never resolves heights again");
  if (ready.empty()) { return false; }
  const auto nearSource = BuildSource(fixture, fixture.Eye, 720.0);
  CHECK(nearSource.has_value(), "near source request lands");
  if (!nearSource) { return false; }
  const auto farSource = BuildSource(fixture, {.LongitudeDeg = 9.5, .LatitudeDeg = 49.3274}, 10.0);
  CHECK(nearSource && farSource, "near and remote source requests both land after camera changes");
  if (!farSource) { return false; }
  CHECK(!nearSource->second.RequestedDetail && !farSource->second.RequestedDetail &&
            nearSource->second.FootprintDetails == std::vector{LevelOfDetail::Fine} &&
            farSource->second.FootprintDetails == std::vector{LevelOfDetail::Massed},
        "implicit source products retain nearby detail and aggregate remote buildings");
  CHECK(nearSource->first != 0 && nearSource->first == farSource->first &&
            !nearSource->second.Built.WallRun.empty() && !farSource->second.Built.WallRun.empty() &&
            !farSource->second.Built.RoofRun.empty(),
        "view-dependent fallback keeps source identity and visible remote geometry");
  const auto nearShell = BuildSource(fixture, fixture.Eye, 720.0, LevelOfDetail::Shell);
  const auto farShell = BuildSource(
      fixture, {.LongitudeDeg = 9.5, .LatitudeDeg = 49.3274}, 10.0, LevelOfDetail::Shell);
  CHECK(nearShell && farShell && nearShell->second.Digest == farShell->second.Digest &&
            nearShell->second.RequestedDetail == LevelOfDetail::Shell &&
            farShell->second.RequestedDetail == LevelOfDetail::Shell,
        "explicit LOD products remain independent of camera position and focal length");
  const auto fineSource = BuildSource(fixture, fixture.Eye, 720.0, LevelOfDetail::Fine);
  CHECK(fineSource && fineSource->second.RequestedDetail == LevelOfDetail::Fine &&
            fineSource->second.FootprintDetails == std::vector{LevelOfDetail::Fine},
        "an explicit Fine source request is preserved");
  return fineSource.has_value();
}

void CheckInputRevisions(BuildingQueueFixture &fixture) {
  auto &prints = fixture.Stack.Footprints();
  const int zoom = fixture.DemTile.Zoom;
  const auto spot = HeightField::SpotOf(fixture.Eye, zoom);
  prints.SeenWith({.FocalPx = 720.0});
  fixture.Queue.Clear();
  prints.Release(0);
  fixture.AllowCopy = true;
  CHECK(fixture.Post() == 1, "tile can be posted again after owned reservation release");
  ++fixture.Source.Revision.Value;
  auto ready = fixture.Finish();
  CHECK(ready.empty() && fixture.Queue.Queued() == 0 && fixture.Queue.Discarded() != 0,
        "late whole-tile geometry with revoked DEM never becomes a landing");
  CHECK(fixture.Post() == 0 && fixture.Queue.Queued() == 0,
        "stale instrumented input cannot reserve new work");
  fixture.Field->SetCertificate(
      TerrainCertificate::FromDelivery(fixture.DemTile, std::nullopt, 11));
  fixture.Source.Revision.Value = 12;
  CHECK(fixture.Post() == 0,
        "changed terrain scope rejects a shaped snapshot with no delivery stamps");
  fixture.Field->SetCertificate(
      TerrainCertificate::FromDelivery(fixture.DemTile, std::nullopt, 12));
  CHECK(fixture.Post() == 1, "current shaped scope retries the unreserved tile");
  fixture.Source.Revision.Value = 13;
  ready = fixture.Finish();
  CHECK(ready.empty() && fixture.Queue.Queued() == 0,
        "old shaped completion cannot land from a frozen producer after scope change");
  TerrainCertificate mixed = TerrainCertificate::FromDelivery(fixture.DemTile, std::nullopt, 12);
  mixed.Merge(TerrainCertificate::FromDelivery(fixture.DemTile, std::nullopt, 13));
  CHECK(!mixed.ScopeCurrent(12) && !mixed.ScopeCurrent(13) && !mixed.ScopeCurrent(0),
        "conflicting known scopes cannot validate even without delivery stamps");
  fixture.Source.Revision.Value = 11;
  fixture.Field->SetCertificate(TerrainCertificate::FromDelivery(
      fixture.DemTile, (*fixture.Revisions).CurrentStamp(fixture.DemTile), 11));
  CHECK(fixture.Post() == 1, "re-certified delivery retries without a leaked reservation");
  CHECK((*fixture.Revisions).IssueDeliveryStamp(fixture.DemTile).has_value(),
        "unchanged raster receives another delivery before activation");
  fixture.Field->SetCertificate(TerrainCertificate::FromDelivery(
      fixture.DemTile, (*fixture.Revisions).CurrentStamp(fixture.DemTile), 11));
  ready = fixture.Finish();
  CHECK(ready.size() == 1, "newly certified input completes normally");
  if (!ready.empty()) { fixture.Queue.CommitsLandings(prints, ready); }
  CHECK(fixture.Queue.Queued() == 0 && prints.InputOfTile(0) != nullptr,
        "only validated completion commits accepted footprint metadata");
  const auto *accepted = prints.InputOfTile(0);
  CHECK(accepted && accepted->HeightRevision == fixture.Source.Revision.Value,
        "accepted products retain the exact input revision of their bake");
  CHECK(accepted && accepted->Heights.Zoom == zoom && !accepted->Heights.Fallback &&
            !accepted->Heights.Tiles.empty() && accepted->Heights.Tiles.front().Zoom == spot.Zoom &&
            accepted->Heights.Tiles.front().X == spot.X &&
            accepted->Heights.Tiles.front().Y == spot.Y,
        "whole-tile landing retains captured DEM requests after releasing its bake task");
  const auto takenBefore = prints.IngestedTiles();
  prints.BeginRefinement();
  fixture.Field = std::make_shared<TerrainField>(*fixture.Field);
  std::fill_n(fixture.Field->Data(), 9, 110.0f);
  fixture.Source.Revision.Value = 14;
  CHECK(fixture.Refine() == 0 && fixture.Queue.Queued() == 0 && prints.RefinementRemaining() == 1,
        "refused replacement admission preserves the refinement cursor");
  fixture.Field->SetCertificate(TerrainCertificate::FromDelivery(
      fixture.DemTile, (*fixture.Revisions).CurrentStamp(fixture.DemTile), 14));
  CHECK(fixture.Refine() == 1 && prints.RefinementComplete(),
        "changed terrain posts one replacement and advances the refinement cursor");
  fixture.Source.Revision.Value = 15;
  fixture.Field = std::make_shared<TerrainField>(*fixture.Field);
  std::fill_n(fixture.Field->Data(), 9, 120.0f);
  fixture.Field->SetCertificate(TerrainCertificate::FromDelivery(
      fixture.DemTile, (*fixture.Revisions).CurrentStamp(fixture.DemTile), 15));
  ready = fixture.Finish(true);
  CHECK(ready.empty() && fixture.Queue.Queued() == 0 && prints.RefinementRemaining() == 1,
        "discarded owned replacement returns its tile to the refinement cursor");
  CHECK(fixture.Refine() == 1, "the replacement retries against the new terrain delivery");
  ready = fixture.Finish(true);
  CHECK(ready.size() == 1, "the retried source replacement becomes a validated landing");
  if (!ready.empty()) { fixture.Queue.CommitsLandings(prints, ready); }
  CHECK(prints.RefinementComplete() && prints.IngestedTiles() == takenBefore,
        "replacement retry completes without a second whole-tile reservation");
}

void CheckReservationOwnership(BuildingQueueFixture &fixture) {
  auto &prints = fixture.Stack.Footprints();
  fixture.Source.Revision.Value = 15;
  prints.ResetDerived();
  CHECK(fixture.Post() == 1, "a fresh reservation is captured by its original field");
  ::outshine::Generators::Osm::BuildingField successor = prints.SnapshotAccepted();
  CHECK(successor.ReservationOwner() != prints.ReservationOwner() && successor.IngestedTiles() == 0,
        "successor snapshot drops pending reservations and gets its own owner");
  successor.Take(0);
  bool foreignLanding = false;
  for (int attempt = 0; attempt < 100 && fixture.Queue.Queued() != 0; ++attempt) {
    const auto landing =
        fixture.Queue.NextLandings(fixture.Stack,
                                   successor,
                                   fixture.Eye,
                                   fixture.Source,
                                   1,
                                   StructureBuildQueue::HeightRequirement::AllowFallback);
    CHECK(landing.has_value(), "foreign owner rejection preserves the bake error contract");
    foreignLanding |= landing && !landing->empty();
    (void)fixture.Queue.AwaitSlice(0.02);
  }
  CHECK(!foreignLanding && fixture.Queue.Queued() == 0 && successor.IngestedTiles() == 1,
        "old jobs neither land into nor release another owner's same-tile reservation");
  successor.Release(0);
  CHECK(successor.IngestedTiles() == 0,
        "only the successor owner releases its deliberately colliding reservation");
  prints.ResetDerived();
  CHECK(fixture.Post() == 1, "current field reserves a tile before a producer revision change");
  const auto reservationOwner = prints.ReservationOwner();
  ++fixture.Source.Revision.Value;
  auto ready = fixture.Finish();
  CHECK(ready.empty() && fixture.Queue.Queued() == 0 && prints.IngestedTiles() == 0 &&
            prints.ReservationOwner() == reservationOwner,
        "producer revision changes release the old job's reservation in the same domain");
  fixture.Field->SetCertificate(TerrainCertificate::FromDelivery(
      fixture.DemTile, std::nullopt, fixture.Source.Revision.Value));
  CHECK(fixture.Post() == 1, "new producer revision can reserve the released tile again");
  ready = fixture.Finish();
  CHECK(ready.size() == 1, "new producer revision completes without an orphaned reservation");
  if (!ready.empty()) { fixture.Queue.CommitsLandings(prints, ready); }
}

void CheckBuildingAbsence(BuildingQueueFixture &fixture) {
  auto &prints = fixture.Stack.Footprints();
  auto lineOnly = fixture.Buildings;
  lineOnly.front().Area = false;
  CHECK(fixture.Stack.Declares(lineOnly), "line-only declarations are accepted");
  CHECK(fixture.Stack.AdvanceAt(fixture.Eye, {.IngestTilesMost = 1, .VectorRing = 0}).has_value(),
        "line-only vector input enters the source snapshot");
  size_t terrainCalls = 0;
  fixture.Source.PinField = [&terrainCalls](Data::TileId, HeightField::Block &) {
    ++terrainCalls;
    return false;
  };
  fixture.Source.Sample = [&terrainCalls](LongitudeLatitude) -> std::optional<double> {
    ++terrainCalls;
    return std::nullopt;
  };
  prints.BeginRefinement();
  CHECK(fixture.Refine() == 1, "a vector tile without building polygons needs no DEM reservation");
  auto ready = fixture.Finish(true);
  CHECK(ready.size() == 1 && ready.front().Baked->OccupiedCells == 0 && terrainCalls == 0,
        "empty building geometry lands without invoking terrain resolvers");
  if (!ready.empty()) { fixture.Queue.CommitsLandings(prints, ready); }
  CHECK(StructureBuildQueue::QualifiedSources(fixture.Stack, prints),
        "proven building absence completes the qualified source snapshot");
  CHECK(fixture.Stack.Declares(fixture.Buildings),
        "replacement building declarations are accepted");
  CHECK(fixture.Stack.AdvanceAt(fixture.Eye, {.IngestTilesMost = 1, .VectorRing = 0}).has_value(),
        "a real building polygon replaces the line-only input");
  prints.BeginRefinement();
  CHECK(fixture.Refine() == 0 && !StructureBuildQueue::QualifiedSources(fixture.Stack, prints) &&
            terrainCalls > 0,
        "missing DEM for a real polygon still defers rather than certifying absence");
}

int CheckWholeTileLanding() {
  BuildingQueueFixture fixture;
  if (!fixture.Open() || !CheckCameraIndependentSources(fixture)) { return Report(); }
  CheckInputRevisions(fixture);
  CheckReservationOwnership(fixture);
  CheckBuildingAbsence(fixture);
  return Report();
}
}

int main() {
  return CheckWholeTileLanding();
}
