#include "JobAdmissionFixture.h"
#include "Check.h"
#include "StructureBuildTask.h"
#include "Tasks.h"
#include <atomic>
#include <chrono>
#include <expected>
#include <memory>
#include <semaphore>
#include <thread>
#include <utility>
#include <variant>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace std::chrono_literals;

struct Scratch final : MeshScratch {};

class BlockingMesher final : public StructureMesher {
public:
  [[nodiscard]] std::unique_ptr<MeshScratch> Scratch() const override {
    return std::make_unique<::Scratch>();
  }

  [[nodiscard]] std::expected<void, StructureMeshError>
  Mesh([[maybe_unused]] const StructurePlan &plan,
       [[maybe_unused]] MeshScratch &scratch,
       [[maybe_unused]] Raised &into) const noexcept override {
    Calls.fetch_add(1, std::memory_order_relaxed);
    Entered.release();
    (void)Release.try_acquire_for(5s);
    return std::unexpected(StructureMeshError::UnsupportedFootprint);
  }

  mutable std::atomic<unsigned> Calls{0};
  mutable std::binary_semaphore Entered{0};
  mutable std::binary_semaphore Release{0};
};

StructureBuildTask Task(const StructureMesher &mesher) {
  auto raw = std::make_unique<Generators::RawTile>();
  raw->LatLon = {47, 9, 47, 9.0001, 47.0001, 9.0001, 47.0001, 9};
  raw->Structures = {{.PointCount = 4,
                      .Cell = {.Index = 1, .Footprint = {}},
                      .HeightM = 6,
                      .WallColour = {},
                      .Facade = {}}};
  raw->TileSpanM = 1000;
  raw->Projection.FocalPx = 1000;
  raw->Eye = {.LongitudeDeg = 9, .LatitudeDeg = 47};
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0);
  return {outshine::Test::JobLease(),
          1,
          std::move(raw),
          Ground::HeightField::Of(0, {std::move(block)}),
          std::make_unique<StructureBuildTask::Output>(),
          mesher.Scratch()};
}

void CheckCancelled(const StructureBuildTask &task) {
  const auto &result = task.Result();
  const auto *kind = result.Status
                         ? nullptr
                         : std::get_if<Generators::StructureBakeErrorKind>(&result.Status.error());
  CHECK(kind && *kind == Generators::StructureBakeErrorKind::Cancelled && !result.Tile,
        "a discarded or stopped slice has explicit cancellation and no published tile");
}

struct DiscardSignal {
  std::binary_semaphore &Signal;

  ~DiscardSignal() { Signal.release(); }
};

void QueuedDiscard() {
  std::binary_semaphore entered{0};
  std::binary_semaphore release{0};
  std::binary_semaphore discarded{0};
  auto pool = std::make_unique<Tasks>(1);
  CHECK(pool->PostDetached([&] {
    entered.release();
    (void)release.try_acquire_for(5s);
  }),
        "the shutdown fixture accepts its blocking worker");
  CHECK(entered.try_acquire_for(2s), "the real worker is occupied before the building post");
  const auto mesher = std::make_shared<BlockingMesher>();
  auto task = Task(*mesher);
  task.Start(*pool, mesher);
  auto signal = std::make_shared<DiscardSignal>(discarded);
  CHECK(pool->PostDetached([signal] {}), "a queued discard observer is accepted");
  signal.reset();
  std::thread shutdown([&] { pool.reset(); });
  CHECK(discarded.try_acquire_for(2s), "pool shutdown actually discards queued closures");
  CHECK(task.AwaitCompletion(2) && task.TakeCompletion(),
        "queued discard completes its own waiter while pool destruction is still blocked");
  CHECK(mesher->Calls.load(std::memory_order_relaxed) == 0,
        "a discarded queued slice never invokes the mesher");
  release.release();
  shutdown.join();
  task.Join();
  CHECK(!task.TakeCompletion(), "a completion is consumed exactly once after its pool is gone");
  CheckCancelled(task);
}

void RunningShutdown() {
  auto pool = std::make_unique<Tasks>(1);
  const auto mesher = std::make_shared<BlockingMesher>();
  auto task = Task(*mesher);
  task.Start(*pool, mesher);
  CHECK(mesher->Entered.try_acquire_for(2s), "the real bake is held inside its mesher");
  task.RequestStop();
  std::thread shutdown([&] { pool.reset(); });
  CHECK(!task.AwaitCompletion(0.01), "shutdown cannot finish a slice that still uses its payload");
  mesher->Release.release();
  shutdown.join();
  CHECK(task.AwaitCompletion(2), "completion remains observable after worker-pool destruction");
  task.Join();
  CHECK(!task.Running() && !task.TakeCompletion(), "join consumes only its owned completion");
  CheckCancelled(task);
}
}

int main() {
  QueuedDiscard();
  RunningShutdown();
  return outshine::Test::Report();
}
