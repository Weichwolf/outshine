#include <array>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <new>
#include <limits>
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
  for (size_t coordinate = 0; coordinate < ring.size(); ++coordinate) {
    for (double sign : {-1.0, 1.0}) {
      auto invalidRing = ring;
      invalidRing[coordinate] = sign * (coordinate % 2 == 0 ? 91.0 : 181.0);
      plan.RingLatLon = invalidRing;
      scratch = mesher.Scratch();
      Raised output = previous;
      const size_t allocations = calls.load();
      failAfter = 0;
      const auto invalid = mesher.Mesh(plan, *scratch, output);
      failAfter = -1;
      CHECK(!invalid && invalid.error() == StructureMeshError::InvalidPlan,
            "every out-of-domain geodetic coordinate is rejected before generation");
      CHECK(calls.load() == allocations, "invalid coordinates require no allocation");
      CHECK(output.WallRun == previous.WallRun && output.WallCorners.size() == 3 &&
                output.RoofRun.empty() && output.RoofCorners.empty(),
            "coordinate rejection preserves previous geometry");
    }
  }
  plan.RingLatLon = ring;
  for (double height : {1.0e20, std::numeric_limits<double>::max()}) {
    plan.HeightM = height;
    scratch = mesher.Scratch();
    const size_t allocations = calls.load();
    failAfter = 0;
    const auto oversized = mesher.Mesh(plan, *scratch, previous);
    failAfter = -1;
    CHECK(!oversized && oversized.error() == StructureMeshError::InvalidPlan,
          "unrepresentable storey counts are rejected before shape generation");
    CHECK(calls.load() == allocations, "unrepresentable heights require no allocation");
  }
  plan.HeightM = 6;
  for (double seat : {-1.0e20, 1.0e20}) {
    plan.SeatAslM = seat;
    plan.FootAslM = seat;
    Raised output = previous;
    const auto unrepresentable = mesher.Mesh(plan, *scratch, output);
    CHECK(!unrepresentable && unrepresentable.error() == StructureMeshError::InvalidPlan,
          "unrepresentable vertex coordinates fail the building transaction");
    CHECK(output.WallRun == previous.WallRun && output.WallCorners.size() == 3 &&
              output.RoofRun.empty() && output.RoofCorners.empty(),
          "coordinate conversion failure retains previous geometry");
    plan.SeatAslM = 0;
    plan.FootAslM = 0;
    const auto recovered = mesher.Mesh(plan, *scratch, output);
    CHECK(recovered && output.WallRun == complete.WallRun && output.RoofRun == complete.RoofRun,
          "scratch recovers from coordinate conversion failure");
  }
  const std::array<double, 6> collapsed{47, 9, 47, 9, 47, 9};
  plan.RingLatLon = collapsed;
  const auto unsupported = mesher.Mesh(plan, *scratch, previous);
  CHECK(!unsupported && unsupported.error() == StructureMeshError::UnsupportedFootprint,
        "collapsed finite footprint differs from invalid input");
  plan.RingLatLon = ring;
  plan.HeightM = std::numeric_limits<double>::quiet_NaN();
  const auto nonfinite = mesher.Mesh(plan, *scratch, previous);
  CHECK(!nonfinite && nonfinite.error() == StructureMeshError::InvalidPlan,
        "nonfinite data is not mislabeled as an unsupported shape");
  plan.HeightM = 6;
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
