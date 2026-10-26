#include "SourceBuildingFixture.h"
#include <cstddef>
#include <cstdint>
#include "BuiltinSourceProviders.h"
#include "BuildingMesh.h"
#include "BuildingFoundationJob.h"
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
  void Number(const char * /*what*/, double /*value*/, const char * /*unit*/) override {}

  void Claim(bool /*held*/, const char * /*why*/) override {}

  void Near(double /*got*/,
            double /*want*/,
            double /*within*/,
            const char * /*unit*/,
            const char * /*why*/) override {}

  void Say(const std::string & /*line*/) override {}
};
}

namespace {
using namespace outshine;
using namespace outshine::Ground;
using namespace outshine::Test;
using namespace outshine::Test::SourceBuildingFixture;

std::shared_ptr<const Generators::Osm::SourceSnapshot> ReadSources() {
  auto parsed = outshine::Generators::Osm::XmlReader::Read(
      R"(<osm version="0.6">
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
  if (!parsed) { return {}; }
  const auto source = std::make_shared<outshine::Generators::Osm::SourceSnapshot>(
      outshine::Generators::Osm::SourceSnapshot{
          .Elements = std::move(*parsed),
          .Coverage = {{.WestDeg = kWestDeg,
                        .SouthDeg = kSouthDeg,
                        .EastDeg = kEastDeg,
                        .NorthDeg = kNorthDeg}},
          .Chunks = {
              {.Location = {}, .PayloadSha256 = std::string(64, 'a'), .PinVerified = true}}});
  return source;
}

void CheckContacts(const BuildView &view) {
  const auto &[stack, prints, queue, policy, heights, eye, frame] = view;
  const ::outshine::Generators::WaterAsset water;
  const ::outshine::Generators::Osm::StreetField streets;
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
  Generators::BuildingFoundationJob stamping(frame, prints.Revision(), prints.PinProducts());
  bool stamped = false;
  for (int attempt = 0; attempt < kAttempts && !stamped; ++attempt) {
    auto ready = stamping.Advance({.Generation = prints.Revision(), .UnitsMost = 1});
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
              std::abs(stamps->front().RingEastNorthM[0] - corner.EastM) < kWaitS &&
              std::abs(stamps->front().RingEastNorthM[1] - corner.NorthM) < kWaitS,
          "four paired metre coordinates preserve the original terrain contact");
  }
}

void CheckReplacement(const BuildView &view,
                      const std::shared_ptr<const Generators::Osm::SourceSnapshot> &source) {
  const auto &[stack, prints, queue, policy, heights, eye, frame] = view;
  auto empty = outshine::Generators::Osm::XmlReader::Read(
      R"(<osm version="0.6"/>)", {.DatasetId = "analytic-original-buildings", .Revision = "empty"});
  CHECK(empty.has_value(), "replacement source explicitly contains no buildings");
  if (!empty) { return; }
  const auto emptySource = std::make_shared<outshine::Generators::Osm::SourceSnapshot>(
      outshine::Generators::Osm::SourceSnapshot{
          .Elements = std::move(*empty), .Coverage = source->Coverage, .Chunks = {}});
  bool replacementPrepared = false;
  for (int attempt = 0; attempt < kAttempts && !replacementPrepared; ++attempt) {
    const auto ready =
        queue.PrepareOriginal(emptySource, policy, stack.FinestZoomOf(Data::DataKind::Elevation));
    CHECK(ready.has_value(), "empty original replacement prepares");
    if (!ready) { break; }
    replacementPrepared = *ready;
    if (!replacementPrepared) { (void)queue.AwaitSlice(kWaitS); }
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
  for (int attempt = 0; attempt < kAttempts && !replaced; ++attempt) {
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
      queue.CommitsLandings(prints, *ready);
      replaced = true;
    } else {
      (void)queue.AwaitSlice(kWaitS);
    }
  }
  CHECK(replaced && (prints.FootprintCount() == 0) && queue.SourcesComplete(stack, prints),
        "empty revision removes obsolete buildings without retaining stale geometry");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  using namespace outshine::Test::SourceBuildingFixture;
  const LongitudeLatitude eye{.LongitudeDeg = kEyeLongitudeDeg, .LatitudeDeg = kEyeLatitudeDeg};
  const auto source = ReadSources();
  if (!source) { return Report(); }
  SilentSink sink;
  Data::OfflineTransport wire;
  const auto cache = std::filesystem::temp_directory_path() /
                     ("outshine-original-terrain-" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Tasks sourceCompute(1);
  SurfacePreparation stack;
  Data::SourceProvider terrainProvider;
  terrainProvider.Kind = "terrain";
  const std::array providers{terrainProvider};
  Data::ProviderRegistry registry;
  Generators::RegisterBuiltinSourceProviders(registry);
  CHECK(stack.Open({.BuiltinData = "data", .Sources = cache.string(), .Assets = {}},
                   providers,
                   registry,
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
  prints.TilesSpan(kTileSpanM);
  prints.SeenWith({.FocalPx = kFocalPx});
  Tasks pool(1);
  Generators::BuildingMesh mesher;
  StructureBuildQueue queue;
  queue.Opens(&pool, &mesher);
  const outshine::Generators::Osm::StructurePolicy policy{
      .Heights = {.StoreyHeightM = 3, .BodyHeightM = 9}, .PointWidthM = 2, .PointsMost = 64};
  bool prepared = false;
  for (int attempt = 0; attempt < kAttempts && !prepared; ++attempt) {
    const auto ready =
        queue.PrepareOriginal(source, policy, stack.FinestZoomOf(Data::DataKind::Elevation));
    CHECK(ready.has_value(), "native preparation accepts complete consumed references");
    if (!ready) { break; }
    prepared = *ready;
    if (!prepared) { (void)queue.AwaitSlice(kWaitS); }
  }
  CHECK(prepared, "worker prepares original inputs independently of vector tiles");
  if (!prepared) { return Report(); }
  TerrainFields fields;
  if (!PrepareTerrain(queue, stack, fields)) { return Report(); }
  const StructureBuildQueue::HeightSource heights{
      .Sample = {},
      .PinField =
          [&fields](Data::TileId tile, HeightField::Block &into) {
            const auto found =
                std::ranges::find(fields, tile, &decltype(fields)::value_type::first);
            return found != fields.end() && HeightField::SharesField(found->second, tile, into);
          },
      .Revision = {.Value = 11}};
  const BuildView view{.stack = stack,
                       .prints = prints,
                       .queue = queue,
                       .policy = policy,
                       .heights = heights,
                       .eye = eye,
                       .frame = frame};
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
  for (int attempt = 0; attempt < kAttempts && !landed; ++attempt) {
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
      queue.CommitsLandings(prints, *ready);
      landed = true;
    } else {
      (void)queue.AwaitSlice(kWaitS);
    }
  }
  CHECK(landed && queue.SourcesComplete(stack, prints) && prints.FootprintCount() == 2,
        "complete native objects publish without a vector tile watermark");
  if (!landed) { return Report(); }
  const auto *accepted = prints.InputOfTile(0);
  CHECK(accepted && accepted->Coordinates && accepted->Coordinates->Origin.Provenance &&
            accepted->Coordinates->Origin.Provenance->DatasetId ==
                source->Elements.SourceIdentity().DatasetId &&
            accepted->Coordinates->Sources.size() == 2 && !accepted->Vector,
        "published geometry owns original identity and retains both typed IDs");
  CheckContacts(view);
  CheckReplacement(view, source);
  std::error_code error;
  std::filesystem::remove_all(cache, error);
  return Report();
}
