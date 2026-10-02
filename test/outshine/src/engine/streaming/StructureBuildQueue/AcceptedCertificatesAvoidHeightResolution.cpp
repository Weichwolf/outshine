#include "BuildingMesh.h"
#include "Check.h"
#include "Digest.h"
#include "SurfacePreparation.h"
#include "OfflineTransport.h"
#include "Sink.h"
#include "StructureBuildQueue.h"
#include "TerrainRevisionIndex.h"
#include "StructureCell.h"
#include "TangentFrame.h"
#include "TileGeodesy.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <new>
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
thread_local bool measureAllocations = false;
thread_local size_t allocations = 0;

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
      ("outshine-structure-certificate-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

  ~TemporaryCache() {
    std::error_code error;
    std::filesystem::remove_all(Path, error);
  }
};

}

void *operator new(size_t bytes) {
  if (measureAllocations) { ++allocations; }
  if (void *memory = std::malloc(bytes == 0 ? 1 : bytes)) { return memory; }
  std::abort();
}

void operator delete(void *memory) noexcept {
  std::free(memory);
}

void operator delete(void *memory, size_t) noexcept {
  std::free(memory);
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
  const std::array<OsmField::Declared, 1> buildings{{
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
  prints.SeenWith({.FocalPx = 720});
  const int zoom = stack.FinestZoomOf(Data::DataKind::Elevation);
  std::string demRevision = "one";
  auto revisions = TerrainRevisionIndex::Create(32);
  uint64_t terrainScope = 11;
  const auto spot = HeightField::SpotOf(eye, zoom);
  const Data::TileId demTile{
      .Zoom = zoom, .X = static_cast<uint32_t>(spot.X), .Y = static_cast<uint32_t>(spot.Y)};
  const auto fieldOf = [&demRevision, &revisions, &terrainScope](Data::TileId at) {
    auto field = std::make_shared<TerrainField>(3, 3);
    std::fill_n(field->Data(), 9, 100.0f);
    field->AddSource({.Kind = Data::DataKind::Elevation,
                      .Tile = at,
                      .SourceId = "test-dem",
                      .Revision = demRevision});
    auto stamp = (**revisions).CurrentStamp(at);
    if (!stamp) { stamp = *(**revisions).IssueDeliveryStamp(at); }
    field->SetCertificate(TerrainCertificate::FromDelivery(at, stamp, terrainScope));
    return field;
  };
  std::shared_ptr<const TerrainField> residentField = fieldOf(demTile);
  const auto block = [&residentField](Data::TileId at) {
    HeightField::Block one;
    (void)HeightField::SharesField(residentField, at, one);
    return one;
  };
  auto pinned = HeightField::Of(zoom, {block(demTile)});
  CHECK(pinned && pinned->Qualified() && pinned->Sources().size() == 1,
        "synthetic DEM is a qualified pinned source");
  BuildingField::Baked accepted{.OccupiedCells = uint64_t{1} << (cell->Index - 1u)};
  accepted.CellBounds[cell->Index - 1u] = cell->Footprint;
  accepted.CellMaxHeightM[cell->Index - 1u] = 12.0f;
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
                                          pinned->Certificate());
  prints.CommitAcceptance(std::move(pending), *vectors, accepted);
  const auto sourceKey = StructureBuildQueue::QualifiedSourceKey(prints, 0);
  CHECK(sourceKey && *sourceKey != 0, "accepted tile exposes its qualified source key");
  if (!sourceKey) { return Report(); }
  Tasks pool(1);
  Generators::BuildingMesh mesher;
  StructureBuildQueue queue;
  queue.Opens(&pool, &mesher);
  size_t copiedFields = 0;
  const StructureBuildQueue::HeightSource heights{
      .CopyField =
          [block, &copiedFields](Data::TileId at, HeightField::Block &into) {
            ++copiedFields;
            into = block(at);
            return true;
          },
      .ResidentField = {},
      .CertificateCurrent =
          [&revisions, &terrainScope](const TerrainCertificate &certificate) {
            return certificate.IsComplete() && certificate.TerrainScopeRevision() == terrainScope &&
                   (**revisions).AreCurrent(certificate.Dependencies());
          }};
  CHECK(StructureBuildQueue::ValidateResidentCellSource(stack, prints, heights, 0, *sourceKey),
        "accepted cell source still matches resident DEM and street data");
  CHECK(copiedFields == 0, "valid accepted certificate performs zero field copies");
  auto canonicalSource = heights;
  size_t residentCopies = 0;
  size_t forbiddenResolverCalls = 0;
  canonicalSource.CertificateCurrent = [](const TerrainCertificate &) { return false; };
  canonicalSource.CopyField = [&forbiddenResolverCalls](Data::TileId, HeightField::Block &) {
    ++forbiddenResolverCalls;
    return false;
  };
  canonicalSource.Sample = [&forbiddenResolverCalls](LongitudeLatitude) -> std::optional<double> {
    ++forbiddenResolverCalls;
    return 100.0;
  };
  canonicalSource.ResidentField = [&forbiddenResolverCalls](Data::TileId) {
    ++forbiddenResolverCalls;
    return std::shared_ptr<const TerrainField>{};
  };
  canonicalSource.CopyResidentField = [block, &residentCopies](Data::TileId at,
                                                               HeightField::Block &into) {
    ++residentCopies;
    into = block(at);
    return true;
  };
  CHECK(StructureBuildQueue::ValidateResidentCellSource(
            stack, prints, canonicalSource, 0, *sourceKey) &&
            residentCopies > 0 && forbiddenResolverCalls == 0,
        "resident validation preserves the accepted raster representation without resolution");
  auto changedField = std::make_shared<TerrainField>(3, 3);
  std::fill_n(changedField->Data(), 9, 101.0f);
  for (const auto &source : residentField->Sources()) { changedField->AddSource(source); }
  changedField->SetCertificate(residentField->Certificate());
  canonicalSource.CopyResidentField = [changedField](Data::TileId at, HeightField::Block &into) {
    return HeightField::SharesField(changedField, at, into);
  };
  CHECK(!StructureBuildQueue::ValidateResidentCellSource(
            stack, prints, canonicalSource, 0, *sourceKey),
        "changed resident raster cannot certify the accepted source key");
  canonicalSource.CopyResidentField = [](Data::TileId, HeightField::Block &) { return false; };
  CHECK(!StructureBuildQueue::ValidateResidentCellSource(
            stack, prints, canonicalSource, 0, *sourceKey) &&
            forbiddenResolverCalls == 0,
        "missing canonical resident fields defer without IO or point fallback");
  residentField.reset();
  pinned.reset();
  allocations = 0;
  measureAllocations = true;
  const bool rasterFreeCurrent =
      StructureBuildQueue::ValidateResidentCellSource(stack, prints, heights, 0, *sourceKey);
  measureAllocations = false;
  CHECK(rasterFreeCurrent, "accepted certificate survives eviction of all input rasters");
  CHECK(allocations == 0, "valid certificate hit performs no heap allocation");
  CHECK(copiedFields == 0, "raster-free validation does not prepare terrain");
  auto unrelated = demTile;
  unrelated.X ^= 1u;
  CHECK((**revisions).IssueDeliveryStamp(unrelated).has_value(), "unrelated terrain changes");
  CHECK(StructureBuildQueue::ValidateResidentCellSource(stack, prints, heights, 0, *sourceKey) &&
            copiedFields == 0,
        "another region preserves the zero-work hit");
  terrainScope = 12;
  CHECK(!StructureBuildQueue::ValidateResidentCellSource(stack, prints, heights, 0, *sourceKey),
        "shape scope mismatch rejects accepted terrain");
  CHECK(copiedFields == 0, "readiness never prepares terrain on an invalid certificate");
  terrainScope = 11;
  prints.TilesSpan(2000);
  CHECK(!StructureBuildQueue::ValidateResidentCellSource(stack, prints, heights, 0, *sourceKey),
        "changed tile span rejects the otherwise valid certificate");
  prints.TilesSpan(1000);
  CHECK((**revisions).IssueDeliveryStamp(demTile).has_value(), "contributing terrain changes");
  CHECK(!StructureBuildQueue::ValidateResidentCellSource(stack, prints, heights, 0, *sourceKey),
        "changed DEM delivery rejects the accepted certificate");
  CHECK(copiedFields == 0, "stale readiness does not copy or queue terrain work");
  using Validation = TerrainCertificate::Validation;
  using State = StructureBuildQueue::CellSourceState;
  size_t forbiddenCalls = 0;
  size_t inspections = 0;
  auto inspectedSource = heights;
  inspectedSource.Sample = [&forbiddenCalls](LongitudeLatitude) {
    ++forbiddenCalls;
    return std::optional<double>{};
  };
  inspectedSource.CopyField = [&forbiddenCalls](Data::TileId, HeightField::Block &) {
    ++forbiddenCalls;
    return false;
  };
  inspectedSource.ResidentField = [&forbiddenCalls](Data::TileId) {
    ++forbiddenCalls;
    return std::shared_ptr<const TerrainField>{};
  };
  inspectedSource.CertificateCurrent = [&forbiddenCalls](const TerrainCertificate &) {
    ++forbiddenCalls;
    return false;
  };
  const std::array cases{std::pair{Validation::Current, State::Current},
                         std::pair{Validation::Unknown, State::Unknown},
                         std::pair{Validation::Stale, State::Stale},
                         std::pair{Validation::ScopeChanged, State::ScopeChanged},
                         std::pair{Validation::Pending, State::Pending}};
  for (const auto &[validation, expected] : cases) {
    inspectedSource.InspectCertificate = [validation, &inspections](const TerrainCertificate &) {
      ++inspections;
      return validation;
    };
    allocations = 0;
    measureAllocations = true;
    const auto actual =
        StructureBuildQueue::InspectCellSource(stack, prints, inspectedSource, 0, *sourceKey);
    measureAllocations = false;
    CHECK(
        actual == expected && allocations == 0 && forbiddenCalls == 0,
        "pure source inspection preserves each metadata status without preparation or allocation");
  }
  CHECK(inspections == cases.size(),
        "only the nonwaiting inspection callback consumes valid metadata");
  const std::array<std::string, 1> foreignLayers{"buildings"};
  OsmField foreign(14, foreignLayers);
  VegetationTemplates streetRules;
  CHECK(streetRules.Load("src/assets/world/vegetation.json", stack.Materials()) &&
            foreign.Declare(buildings, eye).has_value(),
        "foreign street context prepares");
  auto &ownedStreets = const_cast<StreetField &>(stack.Ways());
  const StreetField originalStreets = ownedStreets;
  (void)ownedStreets.Ingest(foreign, streetRules);
  const size_t beforeUnknown = inspections;
  inspectedSource.InspectCertificate = [&inspections](const TerrainCertificate &) {
    ++inspections;
    return Validation::Current;
  };
  allocations = 0;
  measureAllocations = true;
  const auto unknownStreet =
      StructureBuildQueue::InspectCellSource(stack, prints, inspectedSource, 0, *sourceKey);
  const bool residentUnknown = StructureBuildQueue::ValidateResidentCellSource(
      stack, prints, inspectedSource, 0, *sourceKey);
  measureAllocations = false;
  CHECK(unknownStreet == State::Unknown && !residentUnknown && allocations == 0 &&
            forbiddenCalls == 0 && inspections == beforeUnknown,
        "foreign street input cannot invoke optimistic inspection or terrain preparation");
  ownedStreets = originalStreets;
  inspectedSource.InspectCertificate = {};
  allocations = 0;
  measureAllocations = true;
  const auto missingInspector =
      StructureBuildQueue::InspectCellSource(stack, prints, inspectedSource, 0, *sourceKey);
  measureAllocations = false;
  CHECK(missingInspector == State::Unknown && allocations == 0 && forbiddenCalls == 0,
        "missing inspector never falls back to blocking validation or a resident resolver");
  inspectedSource.InspectCertificate = [&inspections](const TerrainCertificate &) {
    ++inspections;
    return Validation::Current;
  };
  inspectedSource.TerrainScope = 12;
  CHECK(StructureBuildQueue::InspectCellSource(stack, prints, inspectedSource, 0, *sourceKey) ==
            State::ScopeChanged,
        "known shape mismatch cannot be overridden by an optimistic callback");
  inspectedSource.TerrainScope = 11;
  prints.TilesSpan(2000);
  CHECK(StructureBuildQueue::InspectCellSource(stack, prints, inspectedSource, 0, *sourceKey) ==
            State::Stale,
        "changed span invalidates source metadata before terrain inspection");
  prints.TilesSpan(1000);
  CHECK(StructureBuildQueue::InspectCellSource(
            stack, prints, inspectedSource, 0, *sourceKey ^ 1u) == State::Stale,
        "another source key cannot be certified by a current terrain callback");
  CHECK(StructureBuildQueue::InspectCellSource(stack,
                                               prints,
                                               inspectedSource,
                                               static_cast<uint32_t>(vectors->Tiles().size()),
                                               *sourceKey) == State::Unknown,
        "absent tile has no source certificate");
  CHECK(inspections == cases.size() && forbiddenCalls == 0,
        "invalid source metadata invokes neither certificate callbacks nor preparation");
  const auto *savedInput = prints.InputOfTile(0);
  auto unscopedAcceptance = prints.PrepareAcceptance(
      0,
      accepted,
      savedInput->Sources,
      true,
      savedInput->Vector,
      savedInput->Bake,
      TerrainCertificate::FromDelivery(demTile, (**revisions).CurrentStamp(demTile)));
  prints.ReplaceAcceptance(std::move(unscopedAcceptance), accepted);
  CHECK(StructureBuildQueue::QualifiedSourceKey(prints, 0) == sourceKey,
        "changing only certificate scope preserves the geometry source key");
  allocations = 0;
  measureAllocations = true;
  const auto unscopedState =
      StructureBuildQueue::InspectCellSource(stack, prints, inspectedSource, 0, *sourceKey);
  measureAllocations = false;
  CHECK(unscopedState == State::Unknown && allocations == 0 && inspections == cases.size() &&
            forbiddenCalls == 0,
        "unknown accepted scope cannot become Current through an optimistic callback");
  return Report();
}
