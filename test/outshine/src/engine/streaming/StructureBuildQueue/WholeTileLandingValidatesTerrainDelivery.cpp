#include "BuildingMesh.h"
#include "Check.h"
#include "Digest.h"
#include "GroundStack.h"
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
  GroundStack stack;
  const LongitudeLatitude eye{.LongitudeDeg = 8.5659, .LatitudeDeg = 49.3274};
  const std::array providers{Data::SourceProvider{.Kind = "terrain"}};
  CHECK(stack.Open({.Shipped = "src/assets", .Cache = cache.Path.string()},
                   providers,
                   eye,
                   wire,
                   sink,
                   nullptr,
                   1.0),
        "offline stack opens");
  if (!stack.Opened()) { return Report(); }
  const std::array<OsmField::Declared, 1> buildings{{
      {.Layer = "buildings",
       .Key = "kind",
       .Value = "building",
       .HeightM = 12,
       .Area = true,
       .LatLon = {49.32739, 8.56589, 49.32739, 8.56591, 49.32741, 8.56591, 49.32741, 8.56589}},
  }};
  stack.Declares(buildings);
  CHECK(stack.Restand(eye, {.IngestTilesMost = 1, .VectorRing = 0}).has_value(),
        "declared building enters the vector snapshot");
  const OsmField *vectors = stack.Vectors();
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
  BuildingField &prints = stack.Footprints();
  prints.AnchorAt(TangentFrame::At(eye).OriginEcef());
  prints.TilesSpan(1000);
  prints.SeenWith(720);
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
      .CopyField =
          [&field, &copies, &allowCopy](Data::TileId at, HeightField::Block &into) {
            ++copies;
            return allowCopy && HeightField::SharesField(field, at, into);
          },
      .ResidentField = {},
      .Revision = {.Value = 11},
      .TerrainScope = 11,
      .CertificateCurrent =
          [&revisions](const TerrainCertificate &certificate) {
            return certificate.IsComplete() && certificate.TerrainScopeRevision() == 11 &&
                   (**revisions).AreCurrent(certificate.Dependencies());
          }};
  Tasks pool(1);
  Generators::BuildingMesh mesher;
  StructureBuildQueue queue;
  queue.Opens(&pool, &mesher);
  const auto post = [&] {
    return queue.Posts(
        stack, prints, eye, source, 1, StructureBuildQueue::HeightRequirement::AllowFallback);
  };
  const auto finish = [&] {
    std::vector<StructureBuildQueue::Landing> landings;
    for (int attempt = 0; attempt < 100 && queue.Queued() != 0; ++attempt) {
      auto ready = queue.NextLandings(
          stack, prints, eye, source, 1, StructureBuildQueue::HeightRequirement::AllowFallback);
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
  queue.Clear();
  prints.Release(0);
  allowCopy = true;
  CHECK(post() == 1, "tile can be posted again after owned reservation release");
  CHECK((**revisions).IssueDeliveryStamp(demTile).has_value(), "later DEM delivery revokes input");
  ready = finish();
  CHECK(ready.empty() && queue.Queued() == 0 && queue.Discarded() != 0,
        "late whole-tile geometry with revoked DEM never becomes a landing");
  CHECK(post() == 0 && queue.Queued() == 0, "stale instrumented input cannot reserve new work");
  field->SetCertificate(TerrainCertificate::FromDelivery(demTile, std::nullopt, 11));
  source.TerrainScope = 12;
  CHECK(post() == 0, "changed terrain scope rejects a shaped snapshot with no delivery stamps");
  CHECK(source.Revision.Value == 11, "producer generation remains unchanged across scope changes");
  field->SetCertificate(TerrainCertificate::FromDelivery(demTile, std::nullopt, 12));
  CHECK(post() == 1, "current shaped scope retries the unreserved tile");
  source.TerrainScope = 13;
  ready = finish();
  CHECK(ready.empty() && queue.Queued() == 0,
        "old shaped completion cannot land from a frozen producer after scope change");
  TerrainCertificate mixed = TerrainCertificate::FromDelivery(demTile, std::nullopt, 12);
  mixed.Merge(TerrainCertificate::FromDelivery(demTile, std::nullopt, 13));
  CHECK(!mixed.ScopeCurrent(12) && !mixed.ScopeCurrent(13) && !mixed.ScopeCurrent(0),
        "conflicting known scopes cannot validate even without delivery stamps");
  source.TerrainScope = 11;
  field->SetCertificate(
      TerrainCertificate::FromDelivery(demTile, (**revisions).CurrentStamp(demTile), 11));
  CHECK(post() == 1, "re-certified delivery retries without a leaked reservation");
  CHECK((**revisions).IssueDeliveryStamp(demTile).has_value(),
        "unchanged raster receives another delivery before activation");
  field->SetCertificate(
      TerrainCertificate::FromDelivery(demTile, (**revisions).CurrentStamp(demTile), 11));
  ready = finish();
  CHECK(ready.size() == 1, "newly certified input completes normally");
  if (!ready.empty()) { queue.CommitsLandings(stack, prints, ready); }
  CHECK(queue.Queued() == 0 && prints.InputOfTile(0) != nullptr,
        "only validated completion commits accepted footprint metadata");
  const auto *accepted = prints.InputOfTile(0);
  CHECK(accepted && source.CertificateCurrent(accepted->Terrain),
        "same-key re-resolution transfers the newly validated certificate to acceptance");
  CHECK(accepted && accepted->Heights.Zoom == zoom && !accepted->Heights.Fallback &&
            !accepted->Heights.Tiles.empty() && accepted->Heights.Tiles.front().Zoom == spot.Zoom &&
            accepted->Heights.Tiles.front().X == spot.X &&
            accepted->Heights.Tiles.front().Y == spot.Y,
        "whole-tile landing retains captured DEM requests after releasing its bake task");
  const auto acceptedKey = StructureBuildQueue::QualifiedSourceKey(prints, 0);
  CHECK(acceptedKey.has_value(), "validated tile has a qualified source key");
  source.TerrainScope = 12;
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
          "cell posting rejects frozen inputs even when producer generation is unchanged");
  }
  source.TerrainScope = 11;
  prints.ResetDerived();
  CHECK(post() == 1, "a fresh reservation is captured by its original field");
  BuildingField successor = prints.SnapshotAccepted();
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
  return Report();
}
