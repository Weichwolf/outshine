#include "ClassificationBuild.h"
#include "Check.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Ground;
using namespace outshine::Test;

struct ComputeGate {
  std::shared_ptr<std::atomic<bool>> Open = std::make_shared<std::atomic<bool>>(false);
  std::shared_ptr<std::atomic<bool>> Entered = std::make_shared<std::atomic<bool>>(false);

  explicit ComputeGate(Tasks &pool) {
    CHECK(pool.PostDetached([open = Open, entered = Entered] {
      entered->store(true);
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
      while (!open->load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
      }
    }),
          "the shared worker accepts the occupancy guard");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!Entered->load() && std::chrono::steady_clock::now() < deadline) {
      std::this_thread::yield();
    }
  }

  ~ComputeGate() { Open->store(true); }
};

ClassificationBuild::Job
Job(uint16_t row, ClassGrain grain, ClassificationBuild::SourceRevision source) {
  ClassificationBuild::Job job;
  job.Frame = TangentFrame::At({});
  job.Grain = grain;
  job.Source = source;
  job.Raster.CellM = grain == ClassGrain::Fine ? 2 : 8;
  job.Raster.HalfCells = 4;
  job.Raster.Points = {-64, -64, 64, -64, 64, 64, -64, 64};
  job.Raster.Rings = {{0, 4}};
  job.Raster.Features = {{.FirstRing = 0,
                          .RingCount = 1,
                          .Rank = 1,
                          .ClassRow = row,
                          .Form = Generators::ClassificationRasterizer::Shape::Polygon,
                          .MinE = -64,
                          .MinN = -64,
                          .MaxE = 64,
                          .MaxN = 64}};
  return job;
}

std::shared_ptr<const ClassStructure> Build(ClassificationBuild &builder,
                                            ClassificationBuild::Job job) {
  CHECK(builder.Submit(std::move(job)), "a bounded classification request is queued");
  CHECK(builder.AwaitCompletion(3), "classification reaches a terminal result");
  auto result = builder.Collect();
  CHECK(result && result->Structure && result->Upload,
        "a completed native product and its render upload are delivered together");
  CHECK(!builder.Collect(), "a result cannot be collected twice");
  return result ? result->Structure : nullptr;
}
}

int main() {
  Tasks compute(1);
  ClassificationBuild builder(compute);
  {
    ComputeGate gate(compute);
    CHECK(gate.Entered->load(), "the one compute worker is occupied");
    CHECK(builder.Submit(Job(3, ClassGrain::Fine, {1, 1, 1})),
          "classification waits on that worker");
    CHECK(!builder.AwaitCompletion(0.01) && !builder.Collect(),
          "classification cannot run on an undeclared private thread");
    CHECK(builder.HeapBytes() >= 8 * sizeof(float),
          "queued native input storage belongs to the compute owner's memory accounting");
    builder.Cancel();
    gate.Open->store(true);
    CHECK(builder.AwaitCompletion(3), "queued cancellation completes");
    auto canceled = builder.Collect();
    CHECK(canceled && !canceled->Structure && !canceled->Upload &&
              canceled->Returned.Raster.Points.size() == 8,
          "canceled work returns owned inputs without publishing a product");
  }
  const auto first = Build(builder, Job(3, ClassGrain::Fine, {1, 1, 1}));
  const auto both = Build(builder, Job(7, ClassGrain::Coarse, {1, 1, 1}));
  CHECK(first && both && both->Evaluate(0, 0, nullptr, nullptr) == 3 &&
            both->Evaluate(24, 0, nullptr, nullptr) == 7,
        "fine takes precedence and matching coarse supplies distant coverage");
  const auto changed = Build(builder, Job(5, ClassGrain::Fine, {2, 2, 2}));
  CHECK(changed && changed->Evaluate(24, 0, nullptr, nullptr) == -1,
        "a new frame never inherits an old coarse raster");
  const auto newCoarse = Build(builder, Job(9, ClassGrain::Coarse, {2, 2, 2}));
  const auto coarseRevision = Build(builder, Job(11, ClassGrain::Coarse, {2, 2, 3}));
  CHECK(newCoarse && coarseRevision && coarseRevision->Evaluate(0, 0, nullptr, nullptr) == 5 &&
            coarseRevision->Evaluate(24, 0, nullptr, nullptr) == 11,
        "only a changed tier is invalidated while unaffected fine remains resident");
  CHECK(first && first->Evaluate(0, 0, nullptr, nullptr) == 3,
        "previous immutable products survive later source revisions and scratch reuse");
  {
    ComputeGate gate(compute);
    auto retired = std::make_unique<ClassificationBuild>(compute);
    CHECK(retired->Submit(Job(3, ClassGrain::Fine, {3, 3, 3})), "retirement owns queued inputs");
    const auto began = std::chrono::steady_clock::now();
    retired.reset();
    const auto elapsed = std::chrono::steady_clock::now() - began;
    CHECK((elapsed < std::chrono::duration<double, std::milli>(1000.0 / 60)),
          "controller retirement does not join the occupied compute worker");
  }
  return Report();
}
