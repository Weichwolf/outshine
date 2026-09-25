#include "BuildingMesh.h"
#include "Check.h"
#include "Digest.h"
#include "GroundStack.h"
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
  const auto block = [zoom](Data::TileId at) {
    HeightField::Block one;
    one.At = {.Zoom = zoom, .X = static_cast<long>(at.X), .Y = static_cast<long>(at.Y)};
    one.Raster = {.Side = 3, .Postings = 3};
    one.Nodes.assign(9, 100.0f);
    one.Sources.push_back(
        {.Kind = Data::DataKind::Elevation, .Tile = at, .SourceId = "test-dem", .Revision = "one"});
    return one;
  };
  const auto spot = HeightField::SpotOf(eye, zoom);
  const Data::TileId demTile{
      .Zoom = zoom, .X = static_cast<uint32_t>(spot.X), .Y = static_cast<uint32_t>(spot.Y)};
  auto pinned = HeightField::Of(zoom, {block(demTile)});
  CHECK(pinned && pinned->Qualified() && pinned->Sources().size() == 1,
        "synthetic DEM is a qualified pinned source");
  BuildingField::Baked accepted{.OccupiedCells = uint64_t{1} << (cell->Index - 1u)};
  prints.PreparesAcceptances({.Tiles = 1});
  prints.Take(0);
  auto pending = prints.PrepareAcceptance(0,
                                          accepted,
                                          pinned->Sources(),
                                          true,
                                          tile.Source,
                                          {.HeightRasterDigest = pinned->RasterDigest(),
                                           .StreetDigest = kDigestBasis,
                                           .FocalPx = 720,
                                           .TileSpanM = 1000,
                                           .Eye = eye});
  prints.CommitAcceptance(std::move(pending), *vectors, accepted);
  const auto sourceKey = StructureBuildQueue::QualifiedSourceKey(prints, 0);
  CHECK(sourceKey && *sourceKey != 0, "accepted tile exposes its qualified source key");
  if (!sourceKey) { return Report(); }
  Tasks pool(1);
  Generators::BuildingMesh mesher;
  StructureBuildQueue queue;
  queue.Opens(&pool, &mesher);
  const StructureBuildQueue::HeightSource heights{
      .CopyField = [block](Data::TileId at, HeightField::Block &into) {
        into = block(at);
        return true;
      }};
  prints.BeginRefinement();
  const LongitudeLatitude movedEye{.LongitudeDeg = eye.LongitudeDeg + 0.01,
                                   .LatitudeDeg = eye.LatitudeDeg};
  CHECK(queue.Posts(stack,
                    prints,
                    movedEye,
                    heights,
                    1,
                    StructureBuildQueue::HeightRequirement::FineOnly,
                    LevelOfDetail::Massed) == 1,
        "whole-tile view replacement posts against the accepted source");
  std::vector<StructureBuildQueue::Landing> whole;
  for (int attempt = 0; attempt < 100 && whole.empty(); ++attempt) {
    auto ready = queue.NextLandings(stack,
                                    prints,
                                    movedEye,
                                    heights.Revision,
                                    1,
                                    StructureBuildQueue::HeightRequirement::FineOnly,
                                    LevelOfDetail::Massed);
    CHECK(ready.has_value(), "whole-tile worker completes without a bake error");
    if (!ready) { break; }
    whole = std::move(*ready);
    if (whole.empty()) { (void)queue.AwaitSlice(0.02); }
  }
  CHECK(whole.size() == 1 && whole.front().Footprints.has_value(),
        "whole-tile landing carries prepared semantic acceptance");
  if (whole.size() != 1) { return Report(); }
  queue.CommitsLandings(stack, prints, whole);
  const auto *qualified = prints.InputOfTile(0);
  CHECK(qualified && qualified->OccupiedCells == accepted.OccupiedCells &&
            qualified->CellBounds[cell->Index - 1u] == cell->Footprint &&
            qualified->CellMaxHeightM[cell->Index - 1u] > 0 &&
            StructureBuildQueue::QualifiedSourceKey(prints, 0) == sourceKey,
        "whole-tile acceptance retains the baked cell mask, full bounds and maximum height");
  const uint64_t semanticRevision = prints.Revision();
  const StructureBuildQueue::CellRequest request{
      .Tile = 0, .Cell = cell->Index, .Detail = LevelOfDetail::Massed, .SourceKey = *sourceKey};
  CHECK(
      !queue.PostsCell(stack,
                       prints,
                       eye,
                       heights,
                       {.Tile = 0, .Cell = 0, .Detail = request.Detail, .SourceKey = *sourceKey}) &&
          !queue.PostsCell(stack,
                           prints,
                           eye,
                           heights,
                           {.Tile = 0,
                            .Cell = request.Cell,
                            .Detail = request.Detail,
                            .SourceKey = *sourceKey + 1}),
      "invalid cell and stale source requests do not reserve work");
  CHECK(queue.PostsCell(stack, prints, eye, heights, request) && queue.QueuedCells() == 1 &&
            !queue.PostsCell(stack, prints, eye, heights, request),
        "one explicit cell product occupies the bounded worker slot");
  std::optional<StructureBuildQueue::Landing> landing;
  for (int attempt = 0; attempt < 100 && !landing; ++attempt) {
    auto ready = queue.NextCellLanding(stack, prints, heights.Revision);
    CHECK(ready.has_value(), "cell worker completes without a bake error");
    if (!ready) { break; }
    landing = std::move(*ready);
    if (!landing) { (void)queue.AwaitSlice(0.02); }
  }
  CHECK(landing && landing->Baked && landing->Baked->RequestedCell == request.Cell &&
            landing->Baked->RequestedDetail == request.Detail &&
            landing->Baked->OccupiedCells == accepted.OccupiedCells && !landing->Footprints,
        "landed geometry is source-keyed and has no semantic acceptance payload");
  if (landing) { queue.CommitsCellLanding(*landing); }
  CHECK(queue.QueuedCells() == 0 && prints.Revision() == semanticRevision &&
            StructureBuildQueue::QualifiedSourceKey(prints, 0) == sourceKey,
        "committing render detail leaves accepted footprints and source identity untouched");
  auto staleRequest = request;
  staleRequest.Detail = LevelOfDetail::Fine;
  CHECK(queue.PostsCell(stack, prints, eye, heights, staleRequest),
        "a second detail can start from the same pinned source");
  auto changedVector = tile.Source;
  changedVector.Revision = "two";
  auto revised = prints.PrepareAcceptance(0,
                                          accepted,
                                          pinned->Sources(),
                                          true,
                                          changedVector,
                                          {.HeightRasterDigest = pinned->RasterDigest(),
                                           .StreetDigest = kDigestBasis,
                                           .FocalPx = 720,
                                           .TileSpanM = 1000,
                                           .Eye = eye});
  prints.ReplaceAcceptance(std::move(revised), accepted);
  for (int attempt = 0; attempt < 100 && queue.QueuedCells() != 0; ++attempt) {
    const auto rejected = queue.NextCellLanding(stack, prints, heights.Revision);
    CHECK(rejected && !*rejected, "stale cell product never lands");
    if (queue.QueuedCells() != 0) { (void)queue.AwaitSlice(0.02); }
  }
  CHECK(queue.QueuedCells() == 0 && prints.Revision() > semanticRevision,
        "a changed accepted source retires an in-flight detail without reacceptance");
  auto restored = prints.PrepareAcceptance(0,
                                           accepted,
                                           pinned->Sources(),
                                           true,
                                           tile.Source,
                                           {.HeightRasterDigest = pinned->RasterDigest(),
                                            .StreetDigest = kDigestBasis,
                                            .FocalPx = 720,
                                            .TileSpanM = 1000,
                                            .Eye = eye});
  prints.ReplaceAcceptance(std::move(restored), accepted);
  staleRequest.Detail = LevelOfDetail::Shell;
  CHECK(queue.PostsCell(stack, prints, eye, heights, staleRequest),
        "restored source can request another detail");
  BuildingField::Baked movedCell = accepted;
  movedCell.OccupiedCells = uint64_t{1} << (cell->Index % Generators::kStructureCellsPerTile);
  auto relocated = prints.PrepareAcceptance(0,
                                            movedCell,
                                            pinned->Sources(),
                                            true,
                                            tile.Source,
                                            {.HeightRasterDigest = pinned->RasterDigest(),
                                             .StreetDigest = kDigestBasis,
                                             .FocalPx = 720,
                                             .TileSpanM = 1000,
                                             .Eye = eye});
  prints.ReplaceAcceptance(std::move(relocated), movedCell);
  CHECK(StructureBuildQueue::QualifiedSourceKey(prints, 0) == sourceKey,
        "cell-mask change alone does not forge a source-key change");
  for (int attempt = 0; attempt < 100 && queue.QueuedCells() != 0; ++attempt) {
    const auto rejected = queue.NextCellLanding(stack, prints, heights.Revision);
    CHECK(rejected && !*rejected, "moved source cell never lands under its old address");
    if (queue.QueuedCells() != 0) { (void)queue.AwaitSlice(0.02); }
  }
  CHECK(queue.QueuedCells() == 0, "changed cell occupancy retires the in-flight request");
  return Report();
}
