#include "BuildingMesh.h"
#include "BuildingStampJob.h"
#include "Check.h"
#include "SurfacePreparation.h"
#include "GroundSnapshot.h"
#include "OfflineTransport.h"
#include "OsmXmlReader.h"
#include "Sink.h"
#include "StructureBuildQueue.h"
#include "TangentFrame.h"
#include "TerrainRevisionIndex.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <memory>
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
}

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  const LongitudeLatitude eye{.LongitudeDeg = 8.5659, .LatitudeDeg = 49.3274};
  auto parsed =
      Data::OsmXmlReader::Read(R"(<osm version="0.6">
    <node id="1" lat="49.32739" lon="8.56589"/>
    <node id="2" lat="49.32739" lon="8.56593"/>
    <node id="3" lat="49.32743" lon="8.56593"/>
    <node id="4" lat="49.32743" lon="8.56589"/>
    <way id="10"><nd ref="1"/><nd ref="2"/><nd ref="3"/><nd ref="4"/><nd ref="1"/>
    <tag k="building" v="yes"/><tag k="height" v="12"/></way>
    <way id="11"><nd ref="1"/><nd ref="2"/><nd ref="3"/><nd ref="4"/><nd ref="1"/>
    <tag k="building:part" v="yes"/><tag k="height" v="18"/>
    <tag k="min_height" v="14"/></way></osm>)",
                               {.DatasetId = "analytic-original-buildings", .Revision = "one"});
  CHECK(parsed.has_value(), "two original building objects parse");
  if (!parsed) { return Report(); }
  auto source = std::make_shared<Data::OsmSourceSnapshot>(Data::OsmSourceSnapshot{
      .Elements = std::move(*parsed),
      .Coverage = {{.WestDeg = 8.565899,
                    .SouthDeg = 49.327399,
                    .EastDeg = 8.565901,
                    .NorthDeg = 49.327401}},
      .Chunks = {{.PayloadSha256 = std::string(64, 'a'), .PinVerified = true}}});
  SilentSink sink;
  Data::OfflineTransport wire;
  const auto cache = std::filesystem::temp_directory_path() /
                     ("outshine-original-terrain-" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Tasks sourceCompute(1);
  SurfacePreparation stack;
  const std::array providers{Data::SourceProvider{.Kind = "terrain"}};
  CHECK(stack.Open({.Shipped = "src/assets", .Cache = cache.string()},
                   providers,
                   eye,
                   wire,
                   sourceCompute,
                   sink,
                   nullptr,
                   1.0),
        "analytic fixture stack opens");
  if (!stack.Opened()) { return Report(); }
  CHECK(!stack.Vectors() || stack.Vectors()->Tiles().empty(),
        "no reduced vector feature or tile supplies the original buildings");
  auto &prints = stack.Footprints();
  const TangentFrame frame = TangentFrame::At(eye);
  prints.AnchorAt(frame.OriginEcef());
  prints.TilesSpan(1000);
  prints.SeenWith(720);
  Tasks pool(1);
  Generators::BuildingMesh mesher;
  StructureBuildQueue queue;
  queue.Opens(&pool, &mesher);
  const Generators::OriginalStructurePolicy policy{
      .Heights = {.StoreyHeightM = 3, .BodyHeightM = 9}, .PointWidthM = 2, .PointsMost = 64};
  bool prepared = false;
  for (int attempt = 0; attempt < 100 && !prepared; ++attempt) {
    const auto ready =
        queue.PrepareOriginal(source, policy, stack.FinestZoomOf(Data::DataKind::Elevation));
    CHECK(ready.has_value(), "native preparation accepts complete consumed references");
    if (!ready) { break; }
    prepared = *ready;
    if (!prepared) { (void)queue.AwaitSlice(0.02); }
  }
  CHECK(prepared, "worker prepares original inputs independently of vector tiles");
  if (!prepared) { return Report(); }
  const auto tiles = queue.OriginalHeightTiles(stack.FinestZoomOf(Data::DataKind::Elevation));
  CHECK(tiles && !tiles->empty(), "original geometry requests its own DEM fields");
  if (!tiles || tiles->empty()) { return Report(); }
  auto revisions = TerrainRevisionIndex::Create(128);
  CHECK(revisions.has_value(), "analytic terrain has independent delivery identity");
  if (!revisions) { return Report(); }
  std::vector<std::pair<Data::TileId, std::shared_ptr<TerrainField>>> fields;
  for (const auto tile : *tiles) {
    const auto stamp = (**revisions).IssueDeliveryStamp(tile);
    CHECK(stamp.has_value(), "every demanded terrain block has a delivery");
    if (!stamp) { return Report(); }
    auto field = std::make_shared<TerrainField>(3, 3);
    std::fill_n(field->Data(), 9, 100.0f);
    field->AddSource({.Kind = Data::DataKind::Elevation,
                      .Tile = tile,
                      .SourceId = "analytic-plane",
                      .Revision = "one"});
    field->SetCertificate(TerrainCertificate::FromDelivery(tile, *stamp, 11));
    fields.emplace_back(tile, std::move(field));
  }
  StructureBuildQueue::HeightSource heights{
      .Sample = {},
      .CopyField =
          [&fields](Data::TileId tile, HeightField::Block &into) {
            const auto found =
                std::ranges::find(fields, tile, &decltype(fields)::value_type::first);
            return found != fields.end() && HeightField::SharesField(found->second, tile, into);
          },
      .ResidentField = {},
      .Revision = {.Value = 11},
      .TerrainScope = 11,
      .CertificateCurrent =
          [&revisions](const TerrainCertificate &certificate) {
            return certificate.IsComplete() && certificate.TerrainScopeRevision() == 11 &&
                   (**revisions).AreCurrent(certificate.Dependencies());
          }};
  CHECK(queue.Posts(stack,
                    prints,
                    eye,
                    heights,
                    1,
                    StructureBuildQueue::HeightRequirement::FineOnly,
                    LevelOfDetail::Fine,
                    StructureBuildQueue::BuildPurpose::SourceGeometry) == 1,
        "original source posts to the existing certified structure queue");
  bool landed = false;
  for (int attempt = 0; attempt < 100 && !landed; ++attempt) {
    auto ready = queue.NextLandings(stack,
                                    prints,
                                    eye,
                                    heights,
                                    1,
                                    StructureBuildQueue::HeightRequirement::FineOnly,
                                    LevelOfDetail::Fine,
                                    StructureBuildQueue::BuildPurpose::SourceGeometry);
    CHECK(ready.has_value(), "native mesh bake succeeds");
    if (!ready) { break; }
    if (!ready->empty()) {
      queue.CommitsLandings(stack, prints, *ready);
      landed = true;
    } else {
      (void)queue.AwaitSlice(0.02);
    }
  }
  CHECK(landed && queue.SourcesComplete(stack, prints) && prints.Footprints().size() == 2,
        "complete native objects publish without a vector tile watermark");
  if (!landed) { return Report(); }
  const auto *accepted = prints.InputOfTile(0);
  CHECK(accepted && accepted->Coordinates && accepted->Coordinates->Origin.Provenance &&
            accepted->Coordinates->Origin.Provenance->DatasetId ==
                source->Elements.SourceIdentity().DatasetId &&
            accepted->Coordinates->Sources.size() == 2 && !accepted->Vector,
        "published geometry owns original identity and retains both typed IDs");
  const WaterField water;
  const StreetField streets;
  const Generators::Fields featureSources{
      .Footprints = &prints, .WaterBodies = &water, .Ways = &streets, .BuiltRow = 3};
  const auto featureTile = Generators::Tile::Of(15, eye);
  const auto features = Generators::FeaturesOver(featureTile, featureSources);
  CHECK(features && features->Count() == 2 &&
            features->Contains(features->At(0), featureTile.Enu(eye)),
        "procedural ground features use native product coordinates without a vector field");
  const auto distant = Generators::FeaturesOver(
      Generators::Tile::Of(15, {.LongitudeDeg = 9, .LatitudeDeg = 50}), featureSources);
  CHECK(distant && distant->Count() == 0,
        "native product identity does not alias an unrelated render tile");
  Generators::BuildingStampJob stamping(frame, prints.Revision());
  bool stamped = false;
  for (int attempt = 0; attempt < 100 && !stamped; ++attempt) {
    auto ready = stamping.Advance({.Products = &prints,
                                   .Footprints = prints.Footprints(),
                                   .VectorGeneration = prints.Revision(),
                                   .UnitsMost = 1});
    CHECK(ready.has_value(), "native footprints reach the existing terrain stamp job");
    if (!ready) { break; }
    stamped = *ready;
  }
  auto stamps = std::move(stamping).Take();
  CHECK(stamped && stamps && stamps->size() == 1,
        "grounded object deforms terrain; raised original part adds no ground stamp");
  if (stamps && !stamps->empty()) {
    const auto corner =
        frame.ToLocalPosition({.LongitudeDeg = 8.56589, .LatitudeDeg = 49.32739, .HeightM = 100});
    CHECK(stamps->front().RingEastNorthM.size() == 8 &&
              std::abs(stamps->front().RingEastNorthM[0] - corner.EastM) < 0.02 &&
              std::abs(stamps->front().RingEastNorthM[1] - corner.NorthM) < 0.02,
          "four paired metre coordinates preserve the original terrain contact");
  }
  auto empty = Data::OsmXmlReader::Read(
      R"(<osm version="0.6"/>)", {.DatasetId = "analytic-original-buildings", .Revision = "empty"});
  CHECK(empty.has_value(), "replacement source explicitly contains no buildings");
  if (!empty) { return Report(); }
  auto emptySource = std::make_shared<Data::OsmSourceSnapshot>(
      Data::OsmSourceSnapshot{.Elements = std::move(*empty), .Coverage = source->Coverage});
  bool replacementPrepared = false;
  for (int attempt = 0; attempt < 100 && !replacementPrepared; ++attempt) {
    const auto ready =
        queue.PrepareOriginal(emptySource, policy, stack.FinestZoomOf(Data::DataKind::Elevation));
    CHECK(ready.has_value(), "empty original replacement prepares");
    if (!ready) { break; }
    replacementPrepared = *ready;
    if (!replacementPrepared) { (void)queue.AwaitSlice(0.02); }
  }
  CHECK(replacementPrepared && !queue.SourcesComplete(stack, prints),
        "old nonempty products cannot establish empty-source readiness");
  CHECK(queue.Posts(stack,
                    prints,
                    eye,
                    heights,
                    1,
                    StructureBuildQueue::HeightRequirement::FineOnly,
                    LevelOfDetail::Fine,
                    StructureBuildQueue::BuildPurpose::SourceGeometry) == 1,
        "empty source gets an atomic native product replacement");
  bool replaced = false;
  for (int attempt = 0; attempt < 100 && !replaced; ++attempt) {
    auto ready = queue.NextLandings(stack,
                                    prints,
                                    eye,
                                    heights,
                                    1,
                                    StructureBuildQueue::HeightRequirement::FineOnly,
                                    LevelOfDetail::Fine,
                                    StructureBuildQueue::BuildPurpose::SourceGeometry);
    CHECK(ready.has_value(), "empty replacement has no mesh or DEM requirement");
    if (!ready) { break; }
    if (!ready->empty()) {
      queue.CommitsLandings(stack, prints, *ready);
      replaced = true;
    } else {
      (void)queue.AwaitSlice(0.02);
    }
  }
  CHECK(replaced && prints.Footprints().empty() && queue.SourcesComplete(stack, prints),
        "empty revision removes obsolete buildings without retaining stale geometry");
  std::error_code error;
  std::filesystem::remove_all(cache, error);
  return Report();
}
