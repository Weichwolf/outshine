#include "SourceBuildingFixture.h"
#include <cstddef>
#include <cstdint>
#include "BuiltinSourceProviders.h"
#include "BuildingMesh.h"
#include "BuildingFoundationJob.h"
#include "Check.h"
#include "SurfacePreparation.h"
#include "GroundSnapshot.h"
#include "OfflineSourceTransport.h"
#include "OsmXmlReader.h"
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
using namespace outshine;
using namespace outshine::Ground;
using namespace outshine::Test;
using namespace outshine::Test::SourceBuildingFixture;

std::array<std::shared_ptr<const Generators::Osm::SourceSnapshot>, 2> ReadSources() {
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
  auto relationElements = outshine::Generators::Osm::XmlReader::Read(
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
  if (!relationElements) { return {}; }
  const auto neighbor = std::make_shared<outshine::Generators::Osm::SourceSnapshot>(
      outshine::Generators::Osm::SourceSnapshot{
          .Elements = std::move(*relationElements), .Coverage = source->Coverage, .Chunks = {}});
  const std::array<std::shared_ptr<const outshine::Generators::Osm::SourceSnapshot>, 2> sources{
      source, neighbor};
  return sources;
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

void CheckReplacement(
    const BuildView &view,
    const std::array<std::shared_ptr<const Generators::Osm::SourceSnapshot>, 2> &sources) {
  const auto &[stack, prints, queue, policy, heights, eye, frame] = view;
  const auto &source = sources[0];
  const auto &neighbor = sources[1];
  const auto standalone = std::make_shared<outshine::Generators::Osm::SourceSnapshot>(*source);
  const std::array<std::shared_ptr<const outshine::Generators::Osm::SourceSnapshot>, 2>
      withoutRelation{source, standalone};
  bool changed = false;
  for (int attempt = 0; attempt < kAttempts && !changed; ++attempt) {
    const auto ready = queue.PrepareOriginal(
        withoutRelation, policy, stack.FinestZoomOf(Data::DataKind::Elevation));
    CHECK(ready.has_value(), "changed ownership prepares without changing the first source");
    if (!ready) { return; }
    changed = *ready;
    if (!changed) { (void)queue.AwaitSlice(kWaitS); }
  }
  CHECK(changed && !queue.SourcesComplete(stack, prints),
        "removing a consuming relation invalidates the old selection despite the same source pin");
  for (int attempt = 0; attempt < kAttempts && !queue.SourcesComplete(stack, prints); ++attempt) {
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
    if (!ready) { return; }
    if (ready->empty()) {
      (void)queue.AwaitSlice(kWaitS);
    } else {
      queue.CommitsLandings(prints, *ready);
    }
  }
  const auto *first = prints.InputOfTile(0);
  const auto *next = prints.InputOfTile(1);
  CHECK(queue.SourcesComplete(stack, prints) && prints.FootprintCount() == 2 && first && next &&
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
  const auto conflicting = std::make_shared<outshine::Generators::Osm::SourceSnapshot>(*neighbor);
  const auto *conflictXml = R"(<osm version="0.6">
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
  auto inconsistent = outshine::Generators::Osm::XmlReader::Read(
      different, {.DatasetId = "analytic-original-buildings", .Revision = "one"});
  CHECK(inconsistent.has_value(), "conflicting overlap is syntactically valid original data");
  if (!inconsistent) { return; }
  conflicting->Elements = std::move(*inconsistent);
  const std::array<std::shared_ptr<const outshine::Generators::Osm::SourceSnapshot>, 2> bad{
      source, conflicting};
  bool rejected = false;
  for (int attempt = 0; attempt < kAttempts && !rejected; ++attempt) {
    const auto ready =
        queue.PrepareOriginal(bad, policy, stack.FinestZoomOf(Data::DataKind::Elevation));
    rejected = !ready;
    if (!rejected) { (void)queue.AwaitSlice(kWaitS); }
  }
  CHECK(rejected && prints.Revision() == publishedRevision &&
            queue.SourcesComplete(stack, prints) && prints.FootprintCount() == 2,
        "contradictory overlap rejects the whole candidate and preserves the accepted world");
}

bool CheckLandings(
    const BuildView &view,
    const std::array<std::shared_ptr<const Generators::Osm::SourceSnapshot>, 2> &sources) {
  const auto &[stack, prints, queue, policy, heights, eye, frame] = view;
  const auto &source = sources[0];
  const auto &neighbor = sources[1];
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
    for (int attempt = 0; attempt < kAttempts && !landed; ++attempt) {
      auto ready = queue.NextLandings(stack,
                                      prints,
                                      eye,
                                      heights,
                                      1,
                                      StructureBuildQueue::HeightRequirement::FineOnly,
                                      LevelOfDetail::Fine,
                                      StructureBuildQueue::BuildPurpose::SourceGeometry);
      CHECK(ready.has_value(), "native cell bake succeeds");
      if (!ready) { return false; }
      if (!ready->empty()) {
        queue.CommitsLandings(prints, *ready);
        landed = true;
      } else {
        (void)queue.AwaitSlice(kWaitS);
      }
    }
    CHECK(landed, "cell product lands within the bounded wait");
    if (!landed) { return false; }
  }
  CHECK(queue.SourcesComplete(stack, prints) && prints.FootprintCount() == 2,
        "cross-cell duplicates and consumed member ways produce exactly two buildings");
  const auto *first = prints.InputOfTile(0);
  const auto *next = prints.InputOfTile(1);
  CHECK(first && next && first->Coordinates->Origin.Provenance &&
            first->Coordinates->Origin.Provenance->Cell == source->Cell &&
            next->Coordinates->Origin.Provenance &&
            next->Coordinates->Origin.Provenance->Cell == neighbor->Cell && !first->Vector &&
            !next->Vector,
        "each accepted product identifies its source archive without owning the complete cell");
  const Data::SourceObjectId part{
      .Id = 11, .Kind = static_cast<uint8_t>(outshine::Generators::Osm::ElementKind::Way)};
  const Data::SourceObjectId relation{
      .Id = 20, .Kind = static_cast<uint8_t>(outshine::Generators::Osm::ElementKind::Relation)};
  CHECK(first && next && first->Coordinates->Sources.size() == 1 &&
            next->Coordinates->Sources.size() == 1 && first->Coordinates->Sources[0] == part &&
            next->Coordinates->Sources[0] == relation,
        "typed ownership keeps the raised part and consumes the body way through its relation");
  return true;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  using namespace outshine::Test::SourceBuildingFixture;
  const LongitudeLatitude eye{.LongitudeDeg = kEyeLongitudeDeg, .LatitudeDeg = kEyeLatitudeDeg};
  const auto sources = ReadSources();
  if (!sources[0] || !sources[1]) { return Report(); }

  Data::OfflineSourceTransport wire;
  const auto cache = std::filesystem::temp_directory_path() /
                     ("outshine-original-cell-products-" +
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
        queue.PrepareOriginal(sources, policy, stack.FinestZoomOf(Data::DataKind::Elevation));
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
  if (!CheckLandings(view, sources)) { return Report(); }
  CheckContacts(view);
  CheckReplacement(view, sources);
  std::error_code error;
  std::filesystem::remove_all(cache, error);
  return Report();
}
