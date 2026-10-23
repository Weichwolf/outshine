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
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {
constexpr auto kCameraShiftDeg = 0.01;
constexpr auto kViewWaitSliceS = 0.01;
constexpr auto kWaitSliceS = 0.02;
constexpr auto kAttemptBudget = 100;
constexpr auto kTileSpanM = 1000;
constexpr auto kBuildingHeightM = 12.0f;
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

void CheckViewReplacement(outshine::Ground::SurfacePreparation &stack,
                          const outshine::Generators::Osm::BuildingField &source,
                          const outshine::StructureBuildQueue::HeightSource &heights,
                          outshine::LongitudeLatitude eye) {
  using namespace outshine;
  using namespace outshine::Test;
  auto prints = source.SnapshotAccepted();
  Tasks pool(1);
  Generators::BuildingMesh mesher;
  StructureBuildQueue queue;
  queue.Opens(&pool, &mesher);
  const auto originalKey = StructureBuildQueue::QualifiedSourceKey(prints, 0);
  const auto sourceRevision = prints.Revision();
  bool ready = true;
  const auto viewReady = [&ready](uint32_t) { return ready; };
  const auto post = [&] {
    return queue.Posts(stack,
                       prints,
                       eye,
                       heights,
                       1,
                       StructureBuildQueue::HeightRequirement::FineOnly,
                       std::nullopt,
                       StructureBuildQueue::BuildPurpose::ViewDetail,
                       viewReady);
  };
  prints.BeginRefinement();
  CHECK(post() == 0 && queue.Queued() == 0 && prints.RefinementComplete(),
        "valid published view does not repeat source or geometry work");
  ready = false;
  size_t posted = 0;
  for (int attempt = 0; attempt < kAttemptBudget && posted == 0; ++attempt) {
    posted += post();
    if (posted == 0) { (void)queue.AwaitSlice(kViewWaitSliceS); }
  }
  CHECK(posted == 1 && queue.Queued() == 1,
        "missing view product requests replacement even when its physical source is unchanged");
  if (posted != 1) { return; }
  CHECK(post() == 0 && queue.Posted() == 1,
        "pending view replacement retains its bounded reservation without duplicate work");
  std::vector<StructureBuildQueue::Landing> landed;
  for (int attempt = 0; attempt < kAttemptBudget && landed.empty(); ++attempt) {
    auto next = queue.NextLandings(
        stack, prints, eye, heights, 1, StructureBuildQueue::HeightRequirement::FineOnly);
    CHECK(next.has_value(), "stopped view replacement retains valid source and height inputs");
    if (!next) { return; }
    landed = std::move(*next);
    if (landed.empty()) { (void)queue.AwaitSlice(kViewWaitSliceS); }
  }
  CHECK(landed.size() == 1 && !landed.front().Footprints,
        "view replacement lands without replacing unchanged authoritative source contacts");
  if (landed.empty()) { return; }
  queue.CommitsLandings(prints, landed);
  ready = true;
  CHECK(prints.Revision() == sourceRevision &&
            StructureBuildQueue::QualifiedSourceKey(prints, 0) == originalKey &&
            queue.Complete(stack, prints, eye, viewReady) && post() == 0 && queue.Posted() == 1,
        "replacement preserves source identity and stops generating once published");
}

}

namespace {
using namespace outshine;
using namespace outshine::Ground;
using namespace outshine::Test;

struct CellScenario {
  const TemporaryCache cache;
  SilentSink sink;
  Data::OfflineTransport wire;
  Tasks sourceCompute{1};
  SurfacePreparation stack;
  const LongitudeLatitude eye{.LongitudeDeg = 8.5659, .LatitudeDeg = 49.3274};
  ::outshine::Generators::Osm::BuildingField &prints = stack.Footprints();
  const ::outshine::Generators::Osm::OsmField::Tile *tile = nullptr;
  Generators::StructureCell cell{};
  std::string demRevision = "one";
  Data::TileId demTile;
  std::shared_ptr<const TerrainField> residentField;
  std::shared_ptr<const HeightField> pinned;
  ::outshine::Generators::Osm::BuildingField::Baked accepted;
  uint64_t sourceKey = 0;
  Tasks pool{1};
  Generators::BuildingMesh mesher;
  StructureBuildQueue queue;
  size_t copiedFields = 0;
  StructureBuildQueue::HeightSource heights;
  LongitudeLatitude laterEye;
  uint64_t semanticRevision = 0;
  StructureBuildQueue::CellRequest request;
  StructureBuildQueue::CellRequest staleRequest;
  size_t copiedBeforeSecondPost = 0;

  std::shared_ptr<const TerrainField> FieldOf(Data::TileId at) const {
    auto field = std::make_shared<TerrainField>(3, 3);
    std::fill_n(field->Data(), 9, 100.0f);
    field->AddSource({.Kind = Data::DataKind::Elevation,
                      .Tile = at,
                      .SourceId = "test-dem",
                      .Revision = demRevision});
    return field;
  }

  HeightField::Block BlockOf(Data::TileId at) const {
    HeightField::Block one;
    (void)HeightField::SharesField(residentField, at, one);
    return one;
  }

  bool CellsReady(uint32_t tileAt) const {
    return tileAt == 0 && StructureBuildQueue::ValidateResidentCellSource(
                              stack, prints, heights, tileAt, sourceKey);
  }

  bool Open() {
    if (!OpenDeclaredBuilding(stack, cache.Path.string(), eye, wire, sourceCompute, sink)) {
      return false;
    }
    const auto *vectors = stack.Vectors();
    tile = &vectors->Tiles().front();
    const auto &ring = vectors->Rings().front();
    const auto located = Generators::StructureCellOf(
        TileBounds({.Zoom = tile->Z,
                    .X = static_cast<uint32_t>(tile->X),
                    .Y = static_cast<uint32_t>(tile->Y)}),
        std::span(vectors->Points().data() + size_t{2} * ring.First, size_t{2} * ring.Count));
    CHECK(located.has_value() && located->Index != 0,
          "declared footprint has a stable spatial cell");
    if (!located) { return false; }
    cell = *located;
    prints.AnchorAt(TangentFrame::At(eye).OriginEcef());
    prints.TilesSpan(kTileSpanM);
    prints.SeenWith({.FocalPx = kFocalPx});
    return true;
  }

  bool Accept() {
    const int zoom = stack.FinestZoomOf(Data::DataKind::Elevation);
    const auto spot = HeightField::SpotOf(eye, zoom);
    demTile = Data::TileId{
        .Zoom = zoom, .X = static_cast<uint32_t>(spot.X), .Y = static_cast<uint32_t>(spot.Y)};
    residentField = FieldOf(demTile);
    pinned = HeightField::Of(zoom, {BlockOf(demTile)});
    CHECK(pinned && pinned->Qualified() && pinned->Sources().size() == 1,
          "synthetic DEM is a qualified pinned source");
    accepted = ::outshine::Generators::Osm::BuildingField::Baked{
        .Coordinates = {},
        .Prints = {},
        .SeatSpreadM = {},
        .AcrossM = {},
        .OccupiedCells = uint64_t{1} << (cell.Index - 1u)};
    accepted.CellBounds[cell.Index - 1u] = cell.Footprint;
    accepted.CellMaxHeightM[cell.Index - 1u] = kBuildingHeightM;
    prints.PreparesAcceptances(1);
    prints.Take(0);
    auto pending = prints.PrepareAcceptance(0,
                                            accepted,
                                            pinned->Sources(),
                                            true,
                                            tile->Source,
                                            {.HeightRasterDigest = pinned->RasterDigest(),
                                             .StreetDigest = kDigestBasis,
                                             .Projection = {.FocalPx = kFocalPx},
                                             .TileSpanM = kTileSpanM,
                                             .Eye = eye,
                                             .EyeEcef = {}});
    prints.CommitAcceptance(std::move(pending), accepted);
    const auto key = StructureBuildQueue::QualifiedSourceKey(prints, 0);
    CHECK(key.has_value() && *key != 0, "accepted tile exposes its qualified source key");
    if (!key) { return false; }
    sourceKey = *key;
    return true;
  }

  void CheckCurrentView() {
    heights = StructureBuildQueue::HeightSource{
        .Sample = {},
        .PinField =
            [this](Data::TileId at, HeightField::Block &into) {
              ++copiedFields;
              into = BlockOf(at);
              return true;
            },
        .ResidentField =
            [this](Data::TileId at) {
              return at == demTile ? residentField : std::shared_ptr<const TerrainField>{};
            },
        .Revision = {}};
    CheckViewReplacement(stack, prints, heights, eye);
    CHECK(StructureBuildQueue::ValidateResidentCellSource(stack, prints, heights, 0, sourceKey),
          "accepted cell source still matches resident DEM and street data");
    prints.BeginRefinement();
    const LongitudeLatitude movedEye{.LongitudeDeg = eye.LongitudeDeg + kCameraShiftDeg,
                                     .LatitudeDeg = eye.LatitudeDeg};
    CHECK(queue.Posts(stack,
                      prints,
                      movedEye,
                      heights,
                      1,
                      StructureBuildQueue::HeightRequirement::FineOnly,
                      LevelOfDetail::Massed) == 0,
          "camera motion preserves a current semantic source without whole-tile rebaking");
    const auto *qualified = prints.InputOfTile(0);
    CHECK(qualified && qualified->OccupiedCells == accepted.OccupiedCells &&
              qualified->CellBounds[cell.Index - 1u] == cell.Footprint &&
              qualified->CellMaxHeightM[cell.Index - 1u] > 0 &&
              StructureBuildQueue::QualifiedSourceKey(prints, 0) == sourceKey,
          "source acceptance retains the baked cell mask, full bounds and maximum height");
    laterEye = LongitudeLatitude{.LongitudeDeg = movedEye.LongitudeDeg + kCameraShiftDeg,
                                 .LatitudeDeg = movedEye.LatitudeDeg};
    CHECK(!queue.Complete(stack, prints, movedEye, [](uint32_t) { return false; }),
          "a near bake eye cannot certify missing view detail");
    CHECK(queue.Complete(stack, prints, laterEye, [this](uint32_t at) { return CellsReady(at); }) &&
              queue.Posts(stack,
                          prints,
                          laterEye,
                          heights,
                          1,
                          StructureBuildQueue::HeightRequirement::FineOnly,
                          std::nullopt,
                          StructureBuildQueue::BuildPurpose::ViewDetail,
                          [this](uint32_t at) { return CellsReady(at); }) == 0,
          "active current cells satisfy view readiness without a redundant whole-tile rebake");
  }

  void CheckLanding() {
    semanticRevision = prints.Revision();
    request = StructureBuildQueue::CellRequest{
        .Tile = 0, .Cell = cell.Index, .Detail = LevelOfDetail::Massed, .SourceKey = sourceKey};
    CHECK(!queue.PostsCell(
              stack,
              prints,
              eye,
              heights,
              {.Tile = 0, .Cell = 0, .Detail = request.Detail, .SourceKey = sourceKey}) &&
              !queue.PostsCell(stack,
                               prints,
                               eye,
                               heights,
                               {.Tile = 0,
                                .Cell = request.Cell,
                                .Detail = request.Detail,
                                .SourceKey = sourceKey + 1}),
          "invalid cell and stale source requests do not reserve work");
    CHECK(queue.PostsCell(stack, prints, eye, heights, request) && queue.QueuedCells() == 1 &&
              queue.CellQueued(request) && !queue.PostsCell(stack, prints, eye, heights, request),
          "one explicit cell product occupies the bounded worker slot");
    std::optional<StructureBuildQueue::Landing> landing;
    for (int attempt = 0; attempt < kAttemptBudget && !landing; ++attempt) {
      auto ready = queue.NextCellLanding(stack, prints, heights);
      CHECK(ready.has_value(), "cell worker completes without a bake error");
      if (!ready) { break; }
      landing = std::move(*ready);
      if (!landing) { (void)queue.AwaitSlice(kWaitSliceS); }
    }
    CHECK(landing && landing->Baked && landing->Baked->RequestedCell == request.Cell &&
              landing->Baked->RequestedDetail == request.Detail &&
              landing->Baked->OccupiedCells == accepted.OccupiedCells && !landing->Footprints,
          "landed geometry is source-keyed and has no semantic acceptance payload");
    CHECK(queue.FastCellValidations() == 1,
          "unchanged resident DEM validates the landing without rebuilding height blocks");
    if (landing) { queue.CommitsCellLanding(*landing); }
    CHECK(queue.QueuedCells() == 0 && !queue.CellQueued(request) &&
              prints.Revision() == semanticRevision &&
              StructureBuildQueue::QualifiedSourceKey(prints, 0) == sourceKey,
          "committing render detail leaves accepted footprints and source identity untouched");
  }

  void ChangeDem() {
    staleRequest = request;
    staleRequest.Detail = LevelOfDetail::Fine;
    copiedBeforeSecondPost = copiedFields;
    CHECK(queue.PostsCell(stack, prints, eye, heights, staleRequest) &&
              copiedFields == copiedBeforeSecondPost,
          "one tile burst reuses its pinned DEM without another field job");
    demRevision = "two";
    residentField = FieldOf(demTile);
    CHECK(!StructureBuildQueue::ValidateResidentCellSource(stack, prints, heights, 0, sourceKey),
          "activation cannot trust an accepted key after live DEM revision changes");
    CHECK(!queue.Complete(stack, prints, laterEye, [this](uint32_t at) { return CellsReady(at); }),
          "stale active cells cannot satisfy a moved camera view");
    for (int attempt = 0; attempt < kAttemptBudget && queue.QueuedCells() != 0; ++attempt) {
      const auto rejected = queue.NextCellLanding(stack, prints, heights);
      CHECK(rejected && !*rejected, "changed live DEM cannot publish an old cell product");
      if (queue.QueuedCells() != 0) { (void)queue.AwaitSlice(kWaitSliceS); }
    }
    CHECK(queue.QueuedCells() == 0 && prints.Revision() == semanticRevision,
          "discarding a stale cell leaves accepted semantics unchanged");
    CHECK(queue.FastCellValidations() == 1 && copiedFields > copiedBeforeSecondPost,
          "changed resident DEM falls back to live source validation and rejects stale geometry");
    CHECK(!queue.PostsCell(stack, prints, eye, heights, staleRequest),
          "a stale landing invalidates the short-lived height pin");
  }

  void ChangeVector() {
    demRevision = "one";
    residentField = FieldOf(demTile);
    CHECK(queue.PostsCell(stack, prints, eye, heights, staleRequest),
          "a second detail can start from the same pinned source");
    auto changedVector = tile->Source;
    changedVector.Revision = "two";
    auto revised = prints.PrepareAcceptance(0,
                                            accepted,
                                            pinned->Sources(),
                                            true,
                                            changedVector,
                                            {.HeightRasterDigest = pinned->RasterDigest(),
                                             .StreetDigest = kDigestBasis,
                                             .Projection = {.FocalPx = kFocalPx},
                                             .TileSpanM = kTileSpanM,
                                             .Eye = eye,
                                             .EyeEcef = {}});
    prints.ReplaceAcceptance(std::move(revised), accepted);
    for (int attempt = 0; attempt < kAttemptBudget && queue.QueuedCells() != 0; ++attempt) {
      const auto rejected = queue.NextCellLanding(stack, prints, heights);
      CHECK(rejected && !*rejected, "stale cell product never lands");
      if (queue.QueuedCells() != 0) { (void)queue.AwaitSlice(kWaitSliceS); }
    }
    CHECK(queue.QueuedCells() == 0 && prints.Revision() > semanticRevision,
          "a changed accepted source retires an in-flight detail without reacceptance");
  }

  void ChangeCell() {
    auto restored = prints.PrepareAcceptance(0,
                                             accepted,
                                             pinned->Sources(),
                                             true,
                                             tile->Source,
                                             {.HeightRasterDigest = pinned->RasterDigest(),
                                              .StreetDigest = kDigestBasis,
                                              .Projection = {.FocalPx = kFocalPx},
                                              .TileSpanM = kTileSpanM,
                                              .Eye = eye,
                                              .EyeEcef = {}});
    prints.ReplaceAcceptance(std::move(restored), accepted);
    staleRequest.Detail = LevelOfDetail::Shell;
    CHECK(queue.PostsCell(stack, prints, eye, heights, staleRequest),
          "restored source can request another detail");
    ::outshine::Generators::Osm::BuildingField::Baked movedCell = accepted;
    movedCell.OccupiedCells = uint64_t{1} << (cell.Index % Generators::kStructureCellsPerTile);
    auto relocated = prints.PrepareAcceptance(0,
                                              movedCell,
                                              pinned->Sources(),
                                              true,
                                              tile->Source,
                                              {.HeightRasterDigest = pinned->RasterDigest(),
                                               .StreetDigest = kDigestBasis,
                                               .Projection = {.FocalPx = kFocalPx},
                                               .TileSpanM = kTileSpanM,
                                               .Eye = eye,
                                               .EyeEcef = {}});
    prints.ReplaceAcceptance(std::move(relocated), movedCell);
    CHECK(StructureBuildQueue::QualifiedSourceKey(prints, 0) == sourceKey,
          "cell-mask change alone does not forge a source-key change");
    for (int attempt = 0; attempt < kAttemptBudget && queue.QueuedCells() != 0; ++attempt) {
      const auto rejected = queue.NextCellLanding(stack, prints, heights);
      CHECK(rejected && !*rejected, "moved source cell never lands under its old address");
      if (queue.QueuedCells() != 0) { (void)queue.AwaitSlice(kWaitSliceS); }
    }
    CHECK(queue.QueuedCells() == 0, "changed cell occupancy retires the in-flight request");
  }

  void Run() {
    if (!Open() || !Accept()) { return; }
    queue.Opens(&pool, &mesher);
    CheckCurrentView();
    CheckLanding();
    ChangeDem();
    ChangeVector();
    ChangeCell();
  }
};
}

int main() {
  CellScenario scenario;
  scenario.Run();
  return outshine::Test::Report();
}
