#include <array>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <new>
#include "../../../src/generators/building/BuildingMesh.h"
#include "Check.h"

namespace {
std::atomic<size_t> calls{0};
thread_local ptrdiff_t failAfter = -1;

struct ForeignScratch final : outshine::MeshScratch {};
}

void *operator new(size_t bytes) {
  calls.fetch_add(1, std::memory_order_relaxed);
  if (failAfter >= 0 && failAfter-- == 0) {
    failAfter = -1;
    throw std::bad_alloc{};
  }
  void *block = std::malloc(bytes == 0 ? 1 : bytes);
  if (block == nullptr) { throw std::bad_alloc{}; }
  return block;
}

void operator delete(void *block) noexcept {
  std::free(block);
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Generators::BuildingMesh mesher;
  const std::array<double, 8> ring{47, 9, 47, 9.0001, 47.0001, 9.0001, 47.0001, 9};
  StructurePlan plan;
  plan.RingLatLon = ring;
  plan.HeightM = 6;
  plan.HeightMeasured = true;
  Raised previous;
  previous.WallCorners.resize(3);
  previous.WallCorners[1].pos[0] = 1;
  previous.WallCorners[2].pos[1] = 1;
  previous.WallRun = {0, 1, 2};
  auto scratch = mesher.Scratch();
  Raised complete = previous;
  const size_t before = calls.load();
  const bool built = static_cast<bool>(mesher.Mesh(plan, *scratch, complete));
  const size_t count = calls.load() - before;
  CHECK(built && complete.WallRun.size() > previous.WallRun.size(),
        "valid building appends geometry");
  CHECK(count > 0, "fault injection observes actual mesh allocations");
  if (!built || count == 0) { return Report(); }
  for (size_t failure : {size_t{0}, count / 2, count - 1}) {
    scratch = mesher.Scratch();
    Raised output = previous;
    failAfter = static_cast<ptrdiff_t>(failure);
    const auto accepted = mesher.Mesh(plan, *scratch, output);
    failAfter = -1;
    CHECK(!accepted && accepted.error() == StructureMeshError::BuildFailed,
          "injected allocation failure is reported");
    CHECK(output.WallCorners.size() == previous.WallCorners.size() &&
              output.WallRun == previous.WallRun && output.RoofCorners.empty() &&
              output.RoofRun.empty(),
          "failure retains all previous buffer sizes and indices");
    if (output.WallCorners.size() == previous.WallCorners.size()) {
      CHECK(std::memcmp(output.WallCorners.data(),
                        previous.WallCorners.data(),
                        previous.WallCorners.size() * sizeof(StoredVertex)) == 0,
            "previous vertex bytes remain unchanged");
    }
    const auto retried = mesher.Mesh(plan, *scratch, output);
    CHECK(retried && output.WallRun == complete.WallRun && output.RoofRun == complete.RoofRun &&
              output.WallCorners.size() == complete.WallCorners.size() &&
              output.RoofCorners.size() == complete.RoofCorners.size(),
          "scratch recovers after failure and reproduces complete mesh topology");
  }
  ForeignScratch foreign;
  const auto incompatible = mesher.Mesh(plan, foreign, previous);
  CHECK(!incompatible && incompatible.error() == StructureMeshError::IncompatibleScratch,
        "foreign scratch is rejected before accessing concrete storage");
  plan.RingLatLon = std::span(ring).first(7);
  const auto malformed = mesher.Mesh(plan, *scratch, previous);
  CHECK(!malformed && malformed.error() == StructureMeshError::InvalidPlan,
        "odd latitude/longitude sequence is rejected");
  CHECK(previous.WallCorners.size() == 3 && previous.WallRun.size() == 3,
        "input rejection preserves earlier geometry");
  return Report();
}
