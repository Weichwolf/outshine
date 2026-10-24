#include "BuildingMesh.h"
#include "Check.h"
#include "DeclaredBuildingFixture.h"
#include "Digest.h"
#include "Heap.h"
#include "SurfacePreparation.h"
#include "OfflineTransport.h"
#include "Sink.h"
#include "StructureBuildQueue.h"
#include "TerrainRevisionIndex.h"
#include "StructureCell.h"
#include "TangentFrame.h"
#include "TileGeodesy.h"

#include <algorithm>
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
constexpr auto kTileSpanM = 1000;
constexpr auto kChangedSurfaceM = 101.0f;
constexpr auto kBuildingHeightM = 12.0f;
constexpr auto kChangedTileSpanM = 2000;
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
      ("outshine-structure-certificate-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

  ~TemporaryCache() {
    std::error_code error;
    std::filesystem::remove_all(Path, error);
  }
};

void CheckVersionedInputs(outshine::Ground::SurfacePreparation &stack,
                          outshine::Generators::Osm::BuildingField &prints,
                          outshine::StructureBuildQueue::HeightSource &heights,
                          outshine::Data::TileId demTile,
                          const auto &revisions,
                          uint64_t terrainScope,
                          const auto &inspect,
                          uint64_t sourceKey,
                          const auto &valid,
                          const size_t &forbiddenCalls) {
  using namespace outshine;
  using namespace outshine::Test;
  using State = StructureBuildQueue::CellSourceState;
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
  prints.TilesSpan(kChangedTileSpanM);
  CHECK(inspect() == State::Stale && !valid(), "changed tile span rejects accepted products");
  prints.TilesSpan(kTileSpanM);
  CHECK(StructureBuildQueue::InspectCellSource(stack, prints, heights, 0, sourceKey ^ 1u) ==
            State::Stale,
        "wrong source key cannot inherit the current revision");
  CHECK(StructureBuildQueue::InspectCellSource(stack, prints, heights, 1, sourceKey) ==
            State::Unknown,
        "missing tile has no accepted input");
}

outshine::Generators::Osm::BuildingField::Baked
AcceptBuilding(outshine::Generators::Osm::BuildingField &prints,
               const outshine::Generators::StructureCell &cell,
               const std::shared_ptr<const outshine::Ground::HeightField> &pinned,
               const outshine::Data::TileSourceIdentity &source,
               outshine::LongitudeLatitude eye,
               uint64_t terrainScope) {
  using namespace outshine;
  ::outshine::Generators::Osm::BuildingField::Baked accepted{.Coordinates = {},
                                                             .Prints = {},
                                                             .SeatSpreadM = {},
                                                             .AcrossM = {},
                                                             .OccupiedCells = uint64_t{1}
                                                                              << (cell.Index - 1u)};
  accepted.CellBounds[cell.Index - 1u] = cell.Footprint;
  accepted.CellMaxHeightM[cell.Index - 1u] = kBuildingHeightM;
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
                                          terrainScope);
  prints.CommitAcceptance(std::move(pending), accepted);
  return accepted;
}

outshine::Generators::Osm::StreetField
ReplaceStreetInputs(outshine::Ground::SurfacePreparation &stack, outshine::LongitudeLatitude eye) {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array<std::string, 1> foreignLayers{"buildings"};
  ::outshine::Generators::Osm::OsmField foreign(14, foreignLayers);
  outshine::Ground::VegetationTemplates streetRules;
  CHECK(streetRules.Load("src/assets/world/vegetation.json", stack.Materials()) &&
            foreign.Declare(DeclaredBuildings(), eye).has_value(),
        "foreign street context prepares");
  auto &streets = const_cast<::outshine::Generators::Osm::StreetField &>(stack.Ways());
  const auto originalStreets = streets;
  (void)streets.Ingest(foreign, streetRules);
  return originalStreets;
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
  const auto &tile = vectors->Tiles().front();
  const auto &ring = vectors->Rings().front();
  const auto cell = Generators::StructureCellOf(
      TileBounds(
          {.Zoom = tile.Z, .X = static_cast<uint32_t>(tile.X), .Y = static_cast<uint32_t>(tile.Y)}),
      std::span(vectors->Points().data() + size_t{2} * ring.First, size_t{2} * ring.Count));
  CHECK(cell.has_value() && cell->Index != 0, "declared footprint has a stable spatial cell");
  if (!cell) { return Report(); }
  ::outshine::Generators::Osm::BuildingField &prints = stack.Footprints();
  prints.AnchorAt(TangentFrame::At(eye).OriginEcef());
  prints.TilesSpan(kTileSpanM);
  prints.SeenWith({.FocalPx = kFocalPx});
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
  auto accepted = AcceptBuilding(prints, *cell, pinned, tile.Source, eye, terrainScope);
  const auto sourceKey = StructureBuildQueue::QualifiedSourceKey(prints, 0);
  CHECK(sourceKey && *sourceKey != 0, "accepted tile exposes its qualified source key");
  if (!sourceKey) { return Report(); }
  size_t forbiddenCalls = 0;
  StructureBuildQueue::HeightSource heights{
      .Sample = {},
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
  CHECK(Heap::ProcessInstrumentationEnabled(), "allocation instrumentation is active");
  constexpr const char *allocationTag = "resident validation";
  const auto allocatedBefore = Heap::TakenUnder(allocationTag);
  bool current = false;
  {
    Heap::Tagged measurement(allocationTag);
    current = inspect() == State::Current && valid();
  }
  CHECK(current && Heap::TakenUnder(allocationTag) == allocatedBefore && forbiddenCalls == 0,
        "accepted immutable inputs survive raster eviction without allocation or resolution");
  CheckVersionedInputs(stack,
                       prints,
                       heights,
                       demTile,
                       revisions,
                       terrainScope,
                       inspect,
                       *sourceKey,
                       valid,
                       forbiddenCalls);
  auto originalStreets = ReplaceStreetInputs(stack, eye);
  auto &streets = const_cast<::outshine::Generators::Osm::StreetField &>(stack.Ways());
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
  std::fill_n(changed->Data(), 9, kChangedSurfaceM);
  residentField = changed;
  CHECK(!valid(), "different raster bytes with the same source tags reject the accepted product");
  heights.PinResidentField = [](Data::TileId, HeightField::Block &) { return false; };
  CHECK(!valid() && forbiddenCalls == 0, "missing unversioned raster never triggers IO");
  return Report();
}
