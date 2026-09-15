#include <array>
#include <cstdlib>
#include <new>
#include <limits>
#include "src/generators/building/BuildingMesh.h"
#include "Check.h"

namespace {
thread_local ptrdiff_t failMapAfter = -1;
thread_local size_t mapFailures = 0;

struct ForeignScratch final : outshine::MeshScratch {};
}

void *operator new(size_t bytes, std::align_val_t alignment, const std::nothrow_t &) noexcept {
  if (failMapAfter >= 0 && failMapAfter-- == 0) {
    failMapAfter = -1;
    ++mapFailures;
    return nullptr;
  }
  const size_t requested = static_cast<size_t>(alignment);
  const size_t supported = requested < sizeof(void *) ? sizeof(void *) : requested;
  void *block = nullptr;
  return posix_memalign(&block, supported, bytes) == 0 ? block : nullptr;
}

void operator delete(void *block, std::align_val_t) noexcept {
  std::free(block);
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
  const bool built = static_cast<bool>(mesher.Mesh(plan, *scratch, complete));
  CHECK(built && complete.WallRun.size() > previous.WallRun.size(),
        "valid building appends geometry");
  if (!built) { return Report(); }
  for (const ptrdiff_t allocation : {ptrdiff_t{0}, ptrdiff_t{1}, ptrdiff_t{2}}) {
    scratch = mesher.Scratch();
    Raised output = previous;
    const size_t failedBefore = mapFailures;
    failMapAfter = allocation;
    const auto rejected = mesher.Mesh(plan, *scratch, output);
    failMapAfter = -1;
    CHECK(mapFailures == failedBefore + 1, "fault injection reached a nonthrowing map allocation");
    CHECK(!rejected && rejected.error() == StructureMeshError::BuildFailed,
          "nonthrowing map failure reaches the building transaction");
    CHECK(output.WallRun == previous.WallRun && output.WallCorners.size() == 3 &&
              output.RoofRun.empty() && output.RoofCorners.empty(),
          "map allocation failure rolls back appended geometry");
    CHECK(mesher.Mesh(plan, *scratch, output).has_value() && output.WallRun == complete.WallRun &&
              output.RoofRun == complete.RoofRun,
          "building scratch recovers after map allocation failure");
  }
  for (size_t coordinate = 0; coordinate < ring.size(); ++coordinate) {
    for (double sign : {-1.0, 1.0}) {
      auto invalidRing = ring;
      invalidRing[coordinate] = sign * (coordinate % 2 == 0 ? 91.0 : 181.0);
      plan.RingLatLon = invalidRing;
      scratch = mesher.Scratch();
      Raised output = previous;
      const auto invalid = mesher.Mesh(plan, *scratch, output);
      CHECK(!invalid && invalid.error() == StructureMeshError::InvalidPlan,
            "every out-of-domain geodetic coordinate is rejected before generation");
      CHECK(output.WallRun == previous.WallRun && output.WallCorners.size() == 3 &&
                output.RoofRun.empty() && output.RoofCorners.empty(),
            "coordinate rejection preserves previous geometry");
    }
  }
  plan.RingLatLon = ring;
  for (double height : {1.0e20, std::numeric_limits<double>::max()}) {
    plan.HeightM = height;
    scratch = mesher.Scratch();
    const auto oversized = mesher.Mesh(plan, *scratch, previous);
    CHECK(!oversized && oversized.error() == StructureMeshError::InvalidPlan,
          "unrepresentable storey counts are rejected before shape generation");
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
  const std::array<double, 5> cornerHeights{};
  for (const size_t heightCount : {size_t{1}, size_t{3}, size_t{5}}) {
    plan.CornerAslM = std::span<const double>(cornerHeights).first(heightCount);
    Raised output = previous;
    const auto invalid = mesher.Mesh(plan, *scratch, output);
    CHECK(!invalid && invalid.error() == StructureMeshError::InvalidPlan,
          "corner heights must match every ring point or be absent");
    CHECK(output.WallRun == previous.WallRun && output.WallCorners.size() == 3 &&
              output.RoofRun.empty() && output.RoofCorners.empty(),
          "invalid height cardinality preserves existing geometry");
  }
  plan.CornerAslM = std::span<const double>(cornerHeights).first(ring.size() / 2);
  Raised level = previous;
  CHECK(mesher.Mesh(plan, *scratch, level).has_value(), "complete corner heights remain supported");
  plan.CornerAslM = {};
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
