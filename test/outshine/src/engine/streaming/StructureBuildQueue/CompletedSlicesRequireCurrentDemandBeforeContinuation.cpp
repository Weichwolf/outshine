#include "BuildingQueueFixture.h"
#include "Check.h"
#include <chrono>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr size_t kStructures = 1024;

bool Start(BuildingQueueFixture &fixture, std::optional<LevelOfDetail> detail) {
  std::vector<Generators::Osm::OsmField::Declared> buildings(kStructures,
                                                             fixture.Buildings.front());
  for (size_t at = 0; at < buildings.size(); ++at) {
    for (size_t coordinate = 1; coordinate < buildings[at].LatLon.size(); coordinate += 2) {
      buildings[at].LatLon[coordinate] += static_cast<double>(at) * 1.0e-8;
    }
  }
  if (!fixture.Open(buildings)) { return false; }
  const auto posted = fixture.Queue.Posts(fixture.Stack,
                                          fixture.Stack.Footprints(),
                                          fixture.Eye,
                                          fixture.Source,
                                          1,
                                          StructureBuildQueue::HeightRequirement::AllowFallback,
                                          detail);
  CHECK(posted == 1, "a dense real building tile enters native generation");
  if (posted == 0) { return false; }
  CHECK(fixture.Queue.AwaitSlice(2.0), "the initial native worker completes its bounded slice");
  fixture.Queue.PollCompletedTasks();
  return true;
}

std::vector<StructureBuildQueue::Landing> Finish(BuildingQueueFixture &fixture,
                                                 Generators::Osm::BuildingField &prints,
                                                 LongitudeLatitude eye,
                                                 const StructureBuildQueue::HeightSource &height,
                                                 std::optional<LevelOfDetail> detail) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (std::chrono::steady_clock::now() < deadline) {
    auto ready = fixture.Queue.NextLandings(fixture.Stack,
                                            prints,
                                            eye,
                                            height,
                                            1,
                                            StructureBuildQueue::HeightRequirement::FineOnly,
                                            detail);
    CHECK(ready, "native continuation preserves its error result");
    if (!ready) { return {}; }
    if (!ready->empty()) { return std::move(*ready); }
    if (!fixture.Queue.AwaitSlice(0.02)) { return {}; }
  }
  return {};
}

enum class Change { Eye, Height, Source, Owner };

void ChangedDemand(Change change) {
  BuildingQueueFixture fixture;
  const std::optional<LevelOfDetail> detail =
      change == Change::Eye ? std::nullopt : std::optional(LevelOfDetail::Fine);
  if (!Start(fixture, detail)) { return; }
  auto eye = fixture.Eye;
  auto height = fixture.Source;
  auto &prints = fixture.Stack.Footprints();
  auto otherOwner = prints.SnapshotAccepted();
  if (change == Change::Eye) { eye.LongitudeDeg += 0.01; }
  if (change == Change::Height) { ++height.Revision.Value; }
  if (change == Change::Source) {
    CHECK(fixture.Stack.Declares(fixture.Buildings), "changed source replaces the original tile");
    CHECK(fixture.Stack.AdvanceAt(fixture.Eye, {.IngestTilesMost = 1, .VectorRing = 0}),
          "the changed source revision becomes current");
  }
  CHECK(Finish(fixture, change == Change::Owner ? otherOwner : prints, eye, height, detail).empty(),
        "revoked source, terrain, ownership or projected view cannot publish stale geometry");
}

void IntrinsicDemandCompletes(bool continueFromPosts) {
  BuildingQueueFixture fixture;
  if (!Start(fixture, LevelOfDetail::Fine)) { return; }
  auto eye = fixture.Eye;
  eye.LongitudeDeg += 0.01;
  auto &prints = fixture.Stack.Footprints();
  prints.SeenWith({.FocalPx = 1440});
  if (continueFromPosts) {
    (void)fixture.Queue.Posts(fixture.Stack,
                              prints,
                              eye,
                              fixture.Source,
                              1,
                              StructureBuildQueue::HeightRequirement::FineOnly,
                              LevelOfDetail::Fine);
  }
  auto ready = Finish(fixture, prints, eye, fixture.Source, LevelOfDetail::Fine);
  CHECK(ready.size() == 1, "intrinsic geometry survives camera translation and projection change");
  if (ready.empty()) { return; }
  const auto &baked = *ready.front().Baked;
  CHECK(!baked.FallbackHeights && baked.UnsupportedMeshes == 0 && !baked.SelectedView &&
            baked.RequestedDetail == LevelOfDetail::Fine && baked.Prints.size() == kStructures &&
            !baked.Built.WallRun.empty() && !baked.Built.RoofRun.empty(),
        "the completed intrinsic tile contains every source building with actual walls and roofs");
  fixture.Queue.CommitsLandings(prints, ready);
  CHECK(prints.OfTile(0).size() == kStructures,
        "the full intrinsic product publishes all semantic building footprints");
}
}

int main() {
  for (const Change change : {Change::Eye, Change::Height, Change::Source, Change::Owner}) {
    ChangedDemand(change);
  }
  IntrinsicDemandCompletes(false);
  IntrinsicDemandCompletes(true);
  return Report();
}
