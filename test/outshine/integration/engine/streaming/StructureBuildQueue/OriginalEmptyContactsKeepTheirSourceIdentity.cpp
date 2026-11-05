#include "JobAdmissionFixture.h"
#include "test/outshine/src/engine/streaming/StructureBuildQueue/BuildingQueueFixture.h"
#include "BuildingMesh.h"
#include "BuiltinSourceProviders.h"
#include "Check.h"
#include "OfflineSourceTransport.h"
#include "OsmXmlReader.h"
#include "SourceBuildingFixture.h"
#include "StructureBuildQueue.h"
#include "SurfacePreparation.h"
#include "TangentFrame.h"
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using namespace outshine::Test::SourceBuildingFixture;
  auto elements = Generators::Osm::XmlReader::Read(
      R"(<osm version="0.6"><node id="1" lat="49.3274" lon="8.5659"/>
      <node id="2" lat="49.3275" lon="8.5659"/>
      <way id="3"><nd ref="1"/><nd ref="2"/><tag k="highway" v="residential"/></way></osm>)",
      {.DatasetId = "original-without-buildings", .Revision = "one"});
  CHECK(elements, "the original source has roads and no building objects");
  if (!elements) { return Report(); }
  const auto source =
      std::make_shared<const Generators::Osm::SourceSnapshot>(Generators::Osm::SourceSnapshot{
          .Elements = std::move(*elements),
          .Coverage = {{.WestDeg = 8.56, .SouthDeg = 49.32, .EastDeg = 8.57, .NorthDeg = 49.34}},
          .Chunks = {}});
  const BuildingQueueCache directory;
  Data::OfflineSourceTransport wire;
  Tasks compute(1);
  Ground::SurfacePreparation stack;
  Data::SourceProvider terrain;
  terrain.Kind = "terrain";
  const std::array providers{terrain};
  Data::ProviderRegistry registry;
  Generators::RegisterBuiltinSourceProviders(registry);
  const LongitudeLatitude eye{.LongitudeDeg = kEyeLongitudeDeg, .LatitudeDeg = kEyeLatitudeDeg};
  CHECK(stack.Open({.BuiltinData = "data", .Sources = directory.Path.string(), .Assets = {}},
                   providers,
                   registry,
                   eye,
                   wire,
                   compute,
                   nullptr,
                   1.0),
        "the isolated source context opens without vector-map input");
  if (!stack.Opened()) { return Report(); }
  auto &prints = stack.Footprints();
  prints.AnchorAt(TangentFrame::At(eye).OriginEcef());
  prints.TilesSpan(kTileSpanM);
  prints.SeenWith({.FocalPx = kFocalPx});
  Tasks worker(1);
  StructureBuildQueue queue;
  queue.Opens(
      &worker, std::make_shared<Generators::BuildingMesh>(), JobAdmissions(), ContextAdmissions());
  const Generators::Osm::StructurePolicy policy{
      .Heights = {.StoreyHeightM = 3, .BodyHeightM = 9}, .PointWidthM = 2, .PointsMost = 64};
  bool prepared = false;
  for (int attempt = 0; attempt < kAttempts && !prepared; ++attempt) {
    const auto ready =
        queue.PrepareOriginal(source, policy, stack.FinestZoomOf(Data::DataKind::Elevation));
    CHECK(ready, "the original source preparation has a result");
    if (!ready) { return Report(); }
    prepared = *ready;
    if (!prepared) { (void)queue.AwaitSlice(kWaitS); }
  }
  CHECK(prepared, "the complete original source is prepared");
  if (!prepared) { return Report(); }
  constexpr auto requirement = StructureBuildQueue::HeightRequirement::AllowFallback;
  constexpr auto purpose = StructureBuildQueue::BuildPurpose::TerrainContacts;
  const StructureBuildQueue::HeightSource heights{
      .Sample = {}, .PinField = {}, .Revision = {.Value = 11}};
  bool landed = false;
  for (int attempt = 0; attempt < kAttempts && !landed; ++attempt) {
    (void)queue.Posts(stack, prints, eye, heights, 1, requirement, std::nullopt, purpose);
    auto ready =
        queue.NextLandings(stack, prints, eye, heights, 1, requirement, std::nullopt, purpose);
    CHECK(ready, "contact generation has a result");
    if (!ready) { return Report(); }
    if (!ready->empty()) {
      CHECK(ready->size() == 1 && ready->front().Baked->Prints.empty() &&
                ready->front().SourceComplete,
            "the known empty original product completes its source coverage");
      queue.CommitsLandings(prints, *ready);
      landed = true;
    } else {
      (void)queue.AwaitSlice(kWaitS);
    }
  }
  CHECK(landed && queue.SourcesComplete(stack, prints) && queue.Discarded() == 0,
        "an unchanged original source is accepted instead of discarded and regenerated");
  CHECK(landed &&
            queue.Posts(stack, prints, eye, heights, 1, requirement, std::nullopt, purpose) == 0 &&
            queue.Queued() == 0,
        "the accepted empty product needs no repeated work");
  return Report();
}
