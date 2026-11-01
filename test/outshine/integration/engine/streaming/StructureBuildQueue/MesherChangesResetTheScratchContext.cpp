#include "test/outshine/src/engine/streaming/StructureBuildQueue/BuildingQueueFixture.h"
#include "BuildingMesh.h"
#include "Check.h"
#include "JobAdmissionFixture.h"
#include <atomic>
#include <expected>
#include <memory>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;

struct Scratch final : MeshScratch {
  Scratch(int identity, std::unique_ptr<MeshScratch> native)
      : Identity(identity), Native(std::move(native)) {}

  int Identity;
  std::unique_ptr<MeshScratch> Native;
};

class Mesher final : public StructureMesher {
public:
  explicit Mesher(int identity) : Identity_(identity) {}

  [[nodiscard]] std::unique_ptr<MeshScratch> Scratch() const override {
    return std::make_unique<::Scratch>(Identity_, Native_.Scratch());
  }

  [[nodiscard]] std::expected<void, StructureMeshError>
  Mesh(const StructurePlan &plan, MeshScratch &scratch, Raised &into) const noexcept override {
    auto &owned = static_cast<::Scratch &>(scratch);
    if (owned.Identity != Identity_) {
      WrongScratch_.fetch_add(1);
      return std::unexpected(StructureMeshError::IncompatibleScratch);
    }
    Calls_.fetch_add(1);
    return Native_.Mesh(plan, *owned.Native, into);
  }

  [[nodiscard]] unsigned MeshCalls() const noexcept { return Calls_.load(); }

  [[nodiscard]] unsigned ScratchMismatches() const noexcept { return WrongScratch_.load(); }

private:
  mutable std::atomic<unsigned> Calls_{0};
  mutable std::atomic<unsigned> WrongScratch_{0};
  int Identity_;
  Generators::BuildingMesh Native_;
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  BuildingQueueFixture fixture;
  if (!fixture.Open()) { return Report(); }
  const auto first = std::make_shared<Mesher>(1);
  const auto second = std::make_shared<Mesher>(2);
  fixture.Queue.Opens(&fixture.Pool, first, JobAdmissions(), ContextAdmissions());
  CHECK(fixture.Post() == 1, "the first mesher builds the declared source");
  auto initial = fixture.Finish();
  CHECK(!initial.empty(), "the first completed product can recycle its actual scratch");
  if (initial.empty()) { return Report(); }
  fixture.Queue.CommitsLandings(fixture.Stack.Footprints(), initial);
  fixture.Queue.Opens(&fixture.Pool, second, JobAdmissions(), ContextAdmissions());
  auto changed = fixture.Buildings;
  changed.front().HeightM = 20;
  CHECK(fixture.Stack.Declares(changed), "a changed source requires the replacement mesher");
  CHECK(fixture.Stack.AdvanceAt(fixture.Eye, {.IngestTilesMost = 1, .VectorRing = 0}),
        "the replacement input becomes current");
  CHECK(fixture.Post() == 1, "the replacement context admits its source");
  auto replacement = fixture.Finish();
  CHECK(!replacement.empty() && first->MeshCalls() > 0 && second->MeshCalls() > 0 &&
            second->ScratchMismatches() == 0,
        "a replacement mesher only receives scratch created for its own context");
  if (!replacement.empty()) {
    fixture.Queue.CommitsLandings(fixture.Stack.Footprints(), replacement);
  }
  return Report();
}
