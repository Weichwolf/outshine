#include "BuildingQueueFixture.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr size_t kStructures =
    StructureBuildTask::StructuresPerRange * StructureBuildTask::RangesPerTask + 17;

class CountingMesher final : public StructureMesher {
public:
  [[nodiscard]] std::unique_ptr<MeshScratch> Scratch() const override { return Backing_.Scratch(); }

  [[nodiscard]] std::expected<void, StructureMeshError>
  Mesh(const StructurePlan &plan, MeshScratch &scratch, Raised &into) const noexcept override {
    Calls.fetch_add(1, std::memory_order_relaxed);
    return Backing_.Mesh(plan, scratch, into);
  }

  mutable std::atomic_size_t Calls{0};

private:
  Generators::BuildingMesh Backing_;
};

bool Start(BuildingQueueFixture &fixture, const CountingMesher &mesher) {
  std::vector<Generators::Osm::OsmField::Declared> buildings(kStructures,
                                                             fixture.Buildings.front());
  for (size_t at = 0; at < buildings.size(); ++at) {
    for (size_t coordinate = 1; coordinate < buildings[at].LatLon.size(); coordinate += 2) {
      buildings[at].LatLon[coordinate] += double(at) * 1.0e-8;
    }
  }
  if (!fixture.Open(buildings)) { return false; }
  fixture.Queue.Opens(&fixture.Pool, &mesher);
  auto &prints = fixture.Stack.Footprints();
  CHECK(fixture.Queue.Posts(fixture.Stack,
                            prints,
                            fixture.Eye,
                            fixture.Source,
                            1,
                            StructureBuildQueue::HeightRequirement::AllowFallback,
                            LevelOfDetail::Fine) == 1,
        "one real building task starts with more than one slice of work");
  CHECK(fixture.Queue.AwaitSlice(2.0), "the first bounded worker slice completes");
  const size_t calls = mesher.Calls.load(std::memory_order_relaxed);
  CHECK(calls > 0 && calls < kStructures,
        "the first slice meshes real geometry but cannot finish the complete input");
  return calls > 0 && calls < kStructures;
}

void CheckPollingDoesNotPost(BuildingQueueFixture &fixture, const CountingMesher &mesher) {
  const size_t before = mesher.Calls.load(std::memory_order_relaxed);
  fixture.Queue.PollCompletedTasks();
  CHECK(!fixture.Queue.AwaitSlice(2.0),
        "completion collection alone cannot post another worker slice");
  CHECK(mesher.Calls.load(std::memory_order_relaxed) == before,
        "no meshing runs before the current-demand admission");
  std::println("STRUCTURE_SLICE first={} after_collection={}",
               before,
               mesher.Calls.load(std::memory_order_relaxed));
  CHECK(fixture.Queue.Queued() == 1 && fixture.Queue.CompletedRanges() > 0,
        "collected partial work remains owned and accounts completed ranges");
}

enum class Change { Eye, Height, Source, Owner };

void ChangedDemand(Change change) {
  const CountingMesher mesher;
  BuildingQueueFixture fixture;
  if (!Start(fixture, mesher)) { return; }
  const size_t firstSlice = mesher.Calls.load(std::memory_order_relaxed);
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
  CheckPollingDoesNotPost(fixture, mesher);
  auto ready = fixture.Queue.NextLandings(fixture.Stack,
                                          change == Change::Owner ? otherOwner : prints,
                                          eye,
                                          height,
                                          1,
                                          StructureBuildQueue::HeightRequirement::FineOnly,
                                          LevelOfDetail::Fine);
  CHECK(ready && ready->empty() && fixture.Queue.Queued() == 0 && fixture.Queue.Discarded() == 1,
        "outdated partial work is retired before any continuation or landing");
  CHECK(mesher.Calls.load(std::memory_order_relaxed) == firstSlice,
        "changed demand spends no extra geometry work after its first completed slice");
}

void CurrentDemandCompletes(bool continueFromPosts) {
  const CountingMesher mesher;
  BuildingQueueFixture fixture;
  if (!Start(fixture, mesher)) { return; }
  CheckPollingDoesNotPost(fixture, mesher);
  auto &prints = fixture.Stack.Footprints();
  if (continueFromPosts) {
    CHECK(fixture.Queue.Posts(fixture.Stack,
                              prints,
                              fixture.Eye,
                              fixture.Source,
                              1,
                              StructureBuildQueue::HeightRequirement::FineOnly,
                              LevelOfDetail::Fine) == 0,
          "posting demand continues the owned job without posting a duplicate");
    CHECK(fixture.Queue.AwaitSlice(2.0),
          "posting current demand admits its existing job's next worker slice");
  }
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  std::vector<StructureBuildQueue::Landing> ready;
  while (ready.empty() && std::chrono::steady_clock::now() < deadline) {
    auto next = fixture.Queue.NextLandings(fixture.Stack,
                                           prints,
                                           fixture.Eye,
                                           fixture.Source,
                                           1,
                                           StructureBuildQueue::HeightRequirement::FineOnly,
                                           LevelOfDetail::Fine);
    CHECK(next, "current-demand continuation preserves the native error contract");
    if (!next) { return; }
    ready = std::move(*next);
    if (ready.empty()) { (void)fixture.Queue.AwaitSlice(2.0); }
  }
  CHECK(ready.size() == 1 && fixture.Queue.Discarded() == 0,
        "valid work reaches its original terminal product without discarded progress");
  CHECK(mesher.Calls.load(std::memory_order_relaxed) == kStructures,
        "every requested building is meshed exactly once over all slices");
  if (ready.empty()) { return; }
  CHECK(!ready.front().Baked->FallbackHeights && ready.front().Baked->UnsupportedMeshes == 0 &&
            !ready.front().Baked->Built.WallRun.empty() &&
            !ready.front().Baked->Built.RoofRun.empty(),
        "positive continuation produces supported real wall and roof geometry");
  fixture.Queue.CommitsLandings(prints, ready);
  CHECK(fixture.Queue.Queued() == 0 && fixture.Queue.Landed() == 1,
        "the current product commits and releases its worker ownership");
}
}

int main() {
  for (const Change change : {Change::Eye, Change::Height, Change::Source, Change::Owner}) {
    ChangedDemand(change);
  }
  CurrentDemandCompletes(false);
  CurrentDemandCompletes(true);
  return Report();
}
