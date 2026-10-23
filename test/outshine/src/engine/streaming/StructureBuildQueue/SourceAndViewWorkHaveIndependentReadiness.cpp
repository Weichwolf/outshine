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
  Data::SourceProvider terrain;
  terrain.Kind = "terrain";
  const std::array providers{terrain};
  CHECK(stack.Open({.Shipped = "src/assets", .Cache = cache.Path.string(), .AssetCache = {}},
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
  CHECK(stack.Declares(buildings), "building declarations are accepted");
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
  CHECK(post() == 1, "initial physical input is queued");
  auto ready = finish();
  CHECK(ready.size() == 1, "initial qualified physical product lands");
  if (ready.empty()) { return Report(); }
  queue.CommitsLandings(prints, ready);
  prints.BeginRefinement();
  for (int attempt = 0; attempt < 100 && !prints.RefinementComplete(); ++attempt) {
    (void)queue.Posts(stack,
                      prints,
                      eye,
                      source,
                      1,
                      StructureBuildQueue::HeightRequirement::FineOnly,
                      std::nullopt,
                      StructureBuildQueue::BuildPurpose::SourceGeometry);
    auto complete = queue.NextLandings(stack,
                                       prints,
                                       eye,
                                       source,
                                       1,
                                       StructureBuildQueue::HeightRequirement::FineOnly,
                                       std::nullopt,
                                       StructureBuildQueue::BuildPurpose::SourceGeometry);
    CHECK(complete.has_value(), "source qualification finishes without an error");
    if (!complete) { return Report(); }
    if (!complete->empty()) { queue.CommitsLandings(prints, *complete); }
    (void)queue.AwaitSlice(0.02);
  }
  CHECK(prints.RefinementComplete() && queue.SourcesComplete(stack, prints),
        "accepted physical inputs have a completed source pass");
  if (!prints.RefinementComplete()) { return Report(); }
  const auto revision = prints.Revision();
  const auto *physical = prints.OfTile(0).data();
  const auto missingView = [](uint32_t) { return false; };
  size_t posted = 0;
  for (int attempt = 0; attempt < 100 && posted == 0; ++attempt) {
    posted = queue.Posts(stack,
                         prints,
                         eye,
                         source,
                         1,
                         StructureBuildQueue::HeightRequirement::FineOnly,
                         std::nullopt,
                         StructureBuildQueue::BuildPurpose::ViewDetail,
                         missingView);
    if (posted == 0) { (void)queue.AwaitSlice(0.02); }
  }
  CHECK(posted == 1 && queue.Queued() == 1,
        "missing current view creates real independent view work");
  CHECK(prints.SourceRefinementComplete() && queue.SourcesComplete(stack, prints),
        "unchanged qualified physical inputs remain ready while their view is pending");
  CHECK(!queue.Complete(stack, prints, eye, missingView),
        "view-independent source readiness cannot certify the missing rendered view");
  CHECK(prints.Revision() == revision && prints.OfTile(0).data() == physical,
        "view demand preserves physical product ownership and semantic revision");
  auto otherOwner = prints.SnapshotAccepted();
  CHECK(otherOwner.SourceRefinementComplete() && !queue.SourcesComplete(stack, otherOwner),
        "a view task with a different reservation owner cannot certify a source snapshot");
  const auto accepted = *prints.InputOfTile(0);
  const auto products = prints.PinProducts();
  const Generators::Osm::BuildingField::Baked physicalInput{
      .Coordinates = accepted.Coordinates,
      .Prints = products.front().Physical->Prints,
      .SeatSpreadM = products.front().Physical->SeatSpreadM,
      .AcrossM = products.front().Physical->AcrossM,
      .OccupiedCells = accepted.OccupiedCells,
      .CellBounds = accepted.CellBounds,
      .CellMaxHeightM = accepted.CellMaxHeightM,
      .CellShellErrorM = accepted.CellShellErrorM};
  const auto replaceInput = [&](bool qualified, Generators::Osm::BuildingField::BakeInputs bake) {
    prints.ReplaceAcceptance(prints.PrepareAcceptance(0,
                                                      physicalInput,
                                                      accepted.Sources,
                                                      qualified,
                                                      accepted.Vector,
                                                      std::move(bake),
                                                      accepted.HeightRevision,
                                                      accepted.Heights),
                             physicalInput);
  };
  auto changedHeight = accepted.Bake;
  changedHeight.HeightRasterDigest ^= 1;
  replaceInput(true, std::move(changedHeight));
  CHECK(prints.InputOfTile(0)->SourceKey != accepted.SourceKey &&
            !queue.SourcesComplete(stack, prints),
        "pending view work cannot certify changed physical height content");
  replaceInput(false, accepted.Bake);
  CHECK(!queue.SourcesComplete(stack, prints),
        "a view task cannot certify an unqualified accepted input");
  replaceInput(true, accepted.Bake);
  CHECK(queue.SourcesComplete(stack, prints),
        "restoring the exact qualified source restores only physical readiness");
  queue.Clear();
  CHECK(queue.SourcesComplete(stack, prints) && !queue.Complete(stack, prints, eye, missingView),
        "discarding view work preserves physical inputs and leaves the view explicitly pending");
  prints.BeginRefinement();
  CHECK(!prints.SourceRefinementComplete() && !queue.SourcesComplete(stack, prints),
        "a fresh physical-source pass still blocks source readiness before any task is queued");
  return Report();
}
