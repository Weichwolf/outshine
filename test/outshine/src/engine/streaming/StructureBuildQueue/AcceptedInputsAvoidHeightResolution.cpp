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
  ::outshine::Generators::Osm::BuildingField::Baked accepted{.OccupiedCells =
                                                                 uint64_t{1} << (cell->Index - 1u)};
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
                                          terrainScope);
  prints.CommitAcceptance(std::move(pending), accepted);
  const auto sourceKey = StructureBuildQueue::QualifiedSourceKey(prints, 0);
  CHECK(sourceKey && *sourceKey != 0, "accepted tile exposes its qualified source key");
  if (!sourceKey) { return Report(); }
  size_t forbiddenCalls = 0;
  StructureBuildQueue::HeightSource heights{
      .PinField =
          [&forbiddenCalls](Data::TileId, HeightField::Block &) {
            ++forbiddenCalls;
            return false;
          },
      .ResidentField = {},
      .Revision = {.Value = terrainScope}};
  using State = StructureBuildQueue::CellSourceState;
  const auto inspect = [&] {
    return StructureBuildQueue::InspectCellSource(stack, prints, heights, 0, *sourceKey);
  };
  const auto valid = [&] {
    return StructureBuildQueue::ValidateResidentCellSource(stack, prints, heights, 0, *sourceKey);
  };
  residentField.reset();
  pinned.reset();
  allocations = 0;
  measureAllocations = true;
  const bool current = inspect() == State::Current && valid();
  measureAllocations = false;
  CHECK(current && allocations == 0 && forbiddenCalls == 0,
        "accepted immutable inputs survive raster eviction without allocation or resolution");
  auto unrelated = demTile;
  unrelated.X ^= 1u;
  CHECK((**revisions).IssueDeliveryStamp(unrelated).has_value(), "unrelated delivery arrives");
  CHECK(valid(), "unrelated delivery does not change accepted immutable inputs");
  CHECK((**revisions).IssueDeliveryStamp(demTile).has_value(), "cached data is delivered again");
  CHECK(valid(), "redelivery alone does not invalidate immutable source data");
  ++heights.Revision.Value;
  CHECK(inspect() == State::ScopeChanged && !valid() && forbiddenCalls == 0,
        "explicit input change rejects old products even when the raster digest is unchanged");
  heights.Revision.Value = terrainScope;
  prints.TilesSpan(2000);
  CHECK(inspect() == State::Stale && !valid(), "changed tile span rejects accepted products");
  prints.TilesSpan(1000);
  CHECK(StructureBuildQueue::InspectCellSource(stack, prints, heights, 0, *sourceKey ^ 1u) ==
            State::Stale,
        "wrong source key cannot inherit the current revision");
  CHECK(StructureBuildQueue::InspectCellSource(stack, prints, heights, 1, *sourceKey) ==
            State::Unknown,
        "missing tile has no accepted input");
  const std::array<std::string, 1> foreignLayers{"buildings"};
  ::outshine::Generators::Osm::OsmField foreign(14, foreignLayers);
  VegetationTemplates streetRules;
  CHECK(streetRules.Load("src/assets/world/vegetation.json", stack.Materials()) &&
            foreign.Declare(buildings, eye).has_value(),
        "foreign street context prepares");
  auto &streets = const_cast<::outshine::Generators::Osm::StreetField &>(stack.Ways());
  const auto originalStreets = streets;
  (void)streets.Ingest(foreign, streetRules);
  CHECK(inspect() == State::Unknown && !valid() && forbiddenCalls == 0,
        "foreign street inputs cannot inherit accepted terrain validity");
  streets = originalStreets;
  const auto *input = prints.InputOfTile(0);
  auto unversioned =
      prints.PrepareAcceptance(0, accepted, input->Sources, true, input->Vector, input->Bake);
  prints.ReplaceAcceptance(std::move(unversioned), accepted);
  heights.Revision.Value = 0;
  CHECK(inspect() == State::Unknown, "two unknown revisions do not establish input identity");
  residentField = fieldOf(demTile);
  size_t residentPins = 0;
  heights.PinResidentField = [block, &residentPins](Data::TileId at, HeightField::Block &into) {
    ++residentPins;
    into = block(at);
    return true;
  };
  CHECK(valid() && residentPins > 0 && forbiddenCalls == 0,
        "unversioned input requires a matching resident raster without IO");
  auto changed = std::make_shared<TerrainField>(*residentField);
  std::fill_n(changed->Data(), 9, 101.0f);
  residentField = changed;
  CHECK(!valid(), "different raster bytes with the same source tags reject the accepted product");
  heights.PinResidentField = [](Data::TileId, HeightField::Block &) { return false; };
  CHECK(!valid() && forbiddenCalls == 0, "missing unversioned raster never triggers IO");
  return Report();
}
