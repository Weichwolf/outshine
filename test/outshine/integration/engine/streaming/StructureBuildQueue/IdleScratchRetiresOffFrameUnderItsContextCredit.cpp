#include "test/outshine/src/engine/streaming/StructureBuildQueue/BuildingQueueFixture.h"
#include "test/outshine/src/engine/streaming/building/StructureBuildTask/RetirementFixture.h"
#include "Check.h"
#include <chrono>
#include <expected>
#include <memory>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;

struct Scratch final : MeshScratch {
  Scratch(std::shared_ptr<Retirement::Control> control, std::unique_ptr<MeshScratch> native)
      : Control(std::move(control)), Native(std::move(native)) {}

  ~Scratch() override {
    Control->FreedOnFrame.store(std::this_thread::get_id() == Control->Frame);
    Control->ScratchEntered.release();
    (void)Control->ReleaseScratch.try_acquire_for(std::chrono::seconds(5));
    Control->Freed.store(true);
  }

  std::shared_ptr<Retirement::Control> Control;
  std::unique_ptr<MeshScratch> Native;
};

class Mesher final : public StructureMesher {
public:
  explicit Mesher(std::shared_ptr<Retirement::Control> control) : Control_(std::move(control)) {}

  [[nodiscard]] std::unique_ptr<MeshScratch> Scratch() const override {
    return std::make_unique<::Scratch>(Control_, Native_.Scratch());
  }

  [[nodiscard]] std::expected<void, StructureMeshError>
  Mesh(const StructurePlan &plan, MeshScratch &scratch, Raised &into) const noexcept override {
    return Native_.Mesh(plan, *static_cast<::Scratch &>(scratch).Native, into);
  }

private:
  std::shared_ptr<Retirement::Control> Control_;
  Generators::BuildingMesh Native_;
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using namespace std::chrono_literals;
  BuildingQueueFixture fixture;
  if (!fixture.Open()) { return Report(); }
  const auto contexts = std::make_shared<JobAdmission>(1);
  const auto jobs = std::make_shared<JobAdmission>(1);
  const auto control = std::make_shared<Retirement::Control>();
  control->Frame = std::this_thread::get_id();
  const auto mesher = std::make_shared<Mesher>(control);
  fixture.Queue.Opens(&fixture.Pool, mesher, jobs, contexts);
  CHECK(fixture.Post() == 1, "the real native mesher builds an admitted product");
  auto landed = fixture.Finish();
  CHECK(!landed.empty(), "the native product is available for landing");
  if (landed.empty()) { return Report(); }
  fixture.Queue.CommitsLandings(fixture.Stack.Footprints(), landed);
  const auto barrier = fixture.Pool.Post([] {});
  CHECK(barrier != Tasks::kNoTask, "the worker accepts the payload retirement barrier");
  if (barrier != Tasks::kNoTask) { fixture.Pool.Wait(barrier); }
  CHECK(jobs->Active() == 0 && contexts->Active() == 1 && !control->Freed.load(),
        "a completed bake returns its job credit while idle scratch retains the context");
  const auto began = std::chrono::steady_clock::now();
  fixture.Queue.Clear();
  CHECK(std::chrono::steady_clock::now() - began < 250ms,
        "Clear returns without running the blocked idle scratch destructor");
  CHECK(control->ScratchEntered.try_acquire_for(5s), "the real idle scratch starts retirement");
  CHECK(!control->FreedOnFrame.load() && contexts->Active() == 1,
        "idle scratch is destroyed off frame while the context credit remains held");
  control->ReleaseScratch.release();
  const auto until = std::chrono::steady_clock::now() + 5s;
  while (contexts->Active() != 0 && std::chrono::steady_clock::now() < until) {
    std::this_thread::yield();
  }
  CHECK(contexts->Active() == 0 && control->Freed.load(),
        "the context returns its credit after releasing its reusable native scratch");
  return Report();
}
