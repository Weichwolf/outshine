#include "BuildingMesh.h"
#include "BuildingStampJob.h"
#include "Check.h"
#include "GroundStack.h"
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
  auto relationElements = Data::OsmXmlReader::Read(
      R"(<osm version="0.6">
    <node id="1" lat="49.32739" lon="8.56589"/>
    <node id="2" lat="49.32739" lon="8.56593"/>
    <node id="3" lat="49.32743" lon="8.56593"/>
    <node id="4" lat="49.32743" lon="8.56589"/>
    <way id="10"><nd ref="1"/><nd ref="2"/><nd ref="3"/><nd ref="4"/><nd ref="1"/>
    <tag k="building" v="yes"/><tag k="height" v="12"/></way>
    <way id="11"><nd ref="1"/><nd ref="2"/><nd ref="3"/><nd ref="4"/><nd ref="1"/>
    <tag k="building:part" v="yes"/><tag k="height" v="18"/>
    <tag k="min_height" v="14"/></way><relation id="20"><member type="way" ref="10" role="outer"/><tag k="type" v="multipolygon"/><tag k="building" v="yes"/><tag k="height" v="12"/></relation></osm>)",
      {.DatasetId = "analytic-original-buildings", .Revision = "one"});
  CHECK(relationElements.has_value(),
        "neighbor includes the same elements and a consuming relation");
  if (!relationElements) { return Report(); }
  auto neighbor = std::make_shared<Data::OsmSourceSnapshot>(Data::OsmSourceSnapshot{
      .Elements = std::move(*relationElements), .Coverage = source->Coverage});
  const std::array<std::shared_ptr<const Data::OsmSourceSnapshot>, 2> sources{source, neighbor};
  SilentSink sink;
  Data::OfflineTransport wire;
  const auto cache = std::filesystem::temp_directory_path() /
                     ("outshine-original-cell-products-" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  GroundStack stack;
  const std::array providers{Data::SourceProvider{.Kind = "terrain"}};
  CHECK(stack.Open({.Shipped = "src/assets", .Cache = cache.string()},
                   providers,
                   eye,
                   wire,
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
        queue.PrepareOriginal(sources, policy, stack.FinestZoomOf(Data::DataKind::Elevation));
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
  for (size_t product = 0; product < sources.size(); ++product) {
    CHECK(!queue.SourcesComplete(stack, prints), "readiness requires every pinned cell product");
    CHECK(queue.Posts(stack,
                      prints,
                      eye,
                      heights,
                      1,
                      StructureBuildQueue::HeightRequirement::FineOnly,
                      LevelOfDetail::Fine,
                      StructureBuildQueue::BuildPurpose::SourceGeometry) == 1,
          "each cell uses the existing native structure queue");
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
      CHECK(ready.has_value(), "native cell bake succeeds");
      if (!ready) { return Report(); }
      if (!ready->empty()) {
        queue.CommitsLandings(stack, prints, *ready);
        landed = true;
      } else {
        (void)queue.AwaitSlice(0.02);
      }
    }
    CHECK(landed, "cell product lands within the bounded wait");
    if (!landed) { return Report(); }
  }
  CHECK(queue.SourcesComplete(stack, prints) && prints.Footprints().size() == 2,
        "cross-cell duplicates and consumed member ways produce exactly two buildings");
  const auto *first = prints.InputOfTile(0);
  const auto *next = prints.InputOfTile(1);
  CHECK(first && next && first->Coordinates->Original.Snapshot == source &&
            next->Coordinates->Original.Snapshot == neighbor && !first->Vector && !next->Vector,
        "each accepted product retains its own original snapshot without a vector tile");
  const Data::OsmElementId part{.Kind = Data::OsmElementKind::Way, .Id = 11};
  const Data::OsmElementId relation{.Kind = Data::OsmElementKind::Relation, .Id = 20};
  CHECK(first && next && first->Coordinates->Sources.size() == 1 &&
            next->Coordinates->Sources.size() == 1 && first->Coordinates->Sources[0] == part &&
            next->Coordinates->Sources[0] == relation,
        "typed ownership keeps the raised part and consumes the body way through its relation");
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
  auto standalone = std::make_shared<Data::OsmSourceSnapshot>(*source);
  const std::array<std::shared_ptr<const Data::OsmSourceSnapshot>, 2> withoutRelation{source,
                                                                                      standalone};
  bool changed = false;
  for (int attempt = 0; attempt < 100 && !changed; ++attempt) {
    const auto ready = queue.PrepareOriginal(
        withoutRelation, policy, stack.FinestZoomOf(Data::DataKind::Elevation));
    CHECK(ready.has_value(), "changed ownership prepares without changing the first source");
    if (!ready) { return Report(); }
    changed = *ready;
    if (!changed) { (void)queue.AwaitSlice(0.02); }
  }
  CHECK(changed && !queue.SourcesComplete(stack, prints),
        "removing a consuming relation invalidates the old selection despite the same source pin");
  for (int attempt = 0; attempt < 100 && !queue.SourcesComplete(stack, prints); ++attempt) {
    (void)queue.Posts(stack,
                      prints,
                      eye,
                      heights,
                      1,
                      StructureBuildQueue::HeightRequirement::FineOnly,
                      LevelOfDetail::Fine,
                      StructureBuildQueue::BuildPurpose::SourceGeometry);
    auto ready = queue.NextLandings(stack,
                                    prints,
                                    eye,
                                    heights,
                                    1,
                                    StructureBuildQueue::HeightRequirement::FineOnly,
                                    LevelOfDetail::Fine,
                                    StructureBuildQueue::BuildPurpose::SourceGeometry);
    CHECK(ready.has_value(), "updated native ownership bakes and lands");
    if (!ready) { return Report(); }
    if (ready->empty()) {
      (void)queue.AwaitSlice(0.02);
    } else {
      queue.CommitsLandings(stack, prints, *ready);
    }
  }
  first = prints.InputOfTile(0);
  next = prints.InputOfTile(1);
  CHECK(queue.SourcesComplete(stack, prints) && prints.Footprints().size() == 2 && first && next &&
            first->Coordinates->Sources.size() == 2 && next->Coordinates->Sources.empty(),
        "unchanged first snapshot regains the body; the duplicate neighbor publishes an empty "
        "product");
  CHECK(queue.Posts(stack,
                    prints,
                    eye,
                    heights,
                    1,
                    StructureBuildQueue::HeightRequirement::FineOnly,
                    LevelOfDetail::Fine,
                    StructureBuildQueue::BuildPurpose::SourceGeometry) == 0 &&
            queue.Queued() == 0,
        "unchanged resident cell products generate no further work");
  const uint64_t publishedRevision = prints.Revision();
  auto conflicting = std::make_shared<Data::OsmSourceSnapshot>(*neighbor);
  auto conflictXml = R"(<osm version="0.6">
    <node id="1" lat="49.32739" lon="8.56589"/>
    <node id="2" lat="49.32739" lon="8.56593"/>
    <node id="3" lat="49.32743" lon="8.56593"/>
    <node id="4" lat="49.32743" lon="8.56589"/>
    <way id="10"><nd ref="1"/><nd ref="2"/><nd ref="3"/><nd ref="4"/><nd ref="1"/>
    <tag k="building" v="yes"/><tag k="height" v="12"/></way>
    <way id="11"><nd ref="1"/><nd ref="2"/><nd ref="3"/><nd ref="4"/><nd ref="1"/>
    <tag k="building:part" v="yes"/><tag k="height" v="18"/>
    <tag k="min_height" v="14"/></way></osm>)";
  std::string different(conflictXml);
  const auto heightAt = different.find("v=\"12\"");
  different.replace(heightAt, 6, "v=\"13\"");
  auto inconsistent = Data::OsmXmlReader::Read(
      different, {.DatasetId = "analytic-original-buildings", .Revision = "one"});
  CHECK(inconsistent.has_value(), "conflicting overlap is syntactically valid original data");
  if (!inconsistent) { return Report(); }
  conflicting->Elements = std::move(*inconsistent);
  const std::array<std::shared_ptr<const Data::OsmSourceSnapshot>, 2> bad{source, conflicting};
  bool rejected = false;
  for (int attempt = 0; attempt < 100 && !rejected; ++attempt) {
    const auto ready =
        queue.PrepareOriginal(bad, policy, stack.FinestZoomOf(Data::DataKind::Elevation));
    rejected = !ready;
    if (!rejected) { (void)queue.AwaitSlice(0.02); }
  }
  CHECK(rejected && prints.Revision() == publishedRevision &&
            queue.SourcesComplete(stack, prints) && prints.Footprints().size() == 2,
        "contradictory overlap rejects the whole candidate and preserves the accepted world");
  std::error_code error;
  std::filesystem::remove_all(cache, error);
  return Report();
}
