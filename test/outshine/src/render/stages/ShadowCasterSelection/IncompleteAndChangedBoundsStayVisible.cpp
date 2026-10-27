#include "Check.h"
#include "ShadowCasterSelection.h"
#include "FrameContext.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr size_t kSphereRecordFloats = 12;
constexpr float kRadius = 0.25f;
constexpr float kFarDepth = 100000;
constexpr float kProjective = 0.1f;
constexpr uint32_t kUncoveredStart = 18;
constexpr std::array<float, 16> kIdentity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

struct Fixture {
  ShadowCasterSelection Selection;
  std::array<DrawBatch, 1> Batches{};
  std::array<GpuPlacement, 1> Rows{};
  std::array<GeometryClusterTemplate, 2> Jobs{
      GeometryClusterTemplate{.SphereIndex = 0, .RelativeFirstIndex = 12, .IndexCount = 3},
      GeometryClusterTemplate{.SphereIndex = 1, .RelativeFirstIndex = 15, .IndexCount = 3}};
  std::array<float, 2 * kSphereRecordFloats> Spheres{};

  Fixture() {
    Batches[0].FirstIndex = 12;
    Batches[0].IndexCount = 6;
    Batches[0].JobCount = 2;
    Rows[0].Current = kIdentity;
    Spheres[3] = Spheres[15] = kRadius;
    Spheres[12] = 10;
  }

  std::span<const ShadowCasterRange> Prepare(bool current = true, uint32_t mainMeshRows = 1) {
    Selection.Prepare(Batches, Jobs, Spheres, Rows, mainMeshRows, current);
    return Selection.Ranges(0);
  }
};

void Coverage() {
  Fixture f;
  CHECK(f.Prepare().size() == 2, "complete source intervals enable fine selection");
  CHECK(f.Selection.Ready() && f.Selection.HeapBytes() >= 2 * sizeof(ShadowCasterRange),
        "prepared bounds expose their actual retained allocation");
  f.Selection.Invalidate();
  CHECK(!f.Selection.Ready() && f.Selection.Ranges(0).empty(),
        "invalidated bounds cannot suppress any draw");
  CHECK(f.Prepare(false).empty(), "a vertex pose invalidates main-mesh bounds");
  CHECK(f.Prepare(false, 0).size() == 2, "independent piece bounds survive a main-mesh pose");
  f.Jobs[1].RelativeFirstIndex = kUncoveredStart;
  CHECK(f.Prepare().empty(), "an uncovered interval cannot drop unknown geometry");
  f.Jobs[1].RelativeFirstIndex = 15;
  f.Jobs[0].RelativeFirstIndex = 15;
  CHECK(f.Prepare().empty(), "an uncovered prefix falls back to the complete batch");
  f.Jobs[0].RelativeFirstIndex = 12;
  f.Batches[0].IndexCount = 9;
  CHECK(f.Prepare().empty(), "an uncovered suffix falls back to the complete batch");
  f.Batches[0].IndexCount = 6;
  f.Jobs[1].IndexCount = 4;
  CHECK(f.Prepare().empty(), "nontriangular intervals remain unqualified");
  f.Jobs[1].IndexCount = 3;
  f.Batches[0].FirstJob = 1;
  CHECK(f.Prepare().empty(), "batch template spans must fit the current resident product");
  f.Batches[0].FirstJob = 0;
  f.Jobs[0].IndexCount = 6;
  CHECK(f.Prepare().size() == 2, "ordered overlapping source intervals remain covered");
  f.Jobs[1].RelativeFirstIndex = 9;
  CHECK(f.Prepare().empty(), "unordered intervals use the original draw");
}

void Bounds() {
  Fixture f;
  FrameContext frame;
  std::ranges::copy(kIdentity, frame.Mvp.begin());
  const BatchFrustum patch(frame, {});
  auto ranges = f.Prepare();
  CHECK(ranges.size() == 2 && !patch.Outside(ranges[0].Bounds) && patch.Outside(ranges[1].Bounds),
        "only the cluster beyond the actual light side is rejected");
  f.Spheres[2] = kFarDepth;
  ranges = f.Prepare();
  CHECK(ranges.size() == 2 && !patch.Outside(ranges[0].Bounds),
        "offscreen depth-clamped casters remain visible without a Z rejection");
  f.Rows[0].Current[12] = -10;
  ranges = f.Prepare();
  CHECK(ranges.size() == 2 && patch.Outside(ranges[0].Bounds) && !patch.Outside(ranges[1].Bounds),
        "accepted instance movement rebuilds bounds in the shader's coordinate system");
  f.Spheres[15] = -1;
  ranges = f.Prepare();
  CHECK(ranges.size() == 2 && !ranges[1].Bounds && !patch.Outside(ranges[1].Bounds),
        "an unknown individual bound is drawn while retaining complete index coverage");
  f.Jobs[1].SphereIndex = std::numeric_limits<uint32_t>::max();
  CHECK(f.Prepare().size() == 2 && !f.Selection.Ranges(0)[1].Bounds,
        "an unavailable sphere is unknown rather than an empty caster");
  f.Batches[0].Instances = 2;
  CHECK(f.Prepare().empty(), "unqualified multi-instance jobs retain the full original draw");
  f.Batches[0].Instances = 1;
  f.Rows[0].Current[3] = kProjective;
  CHECK(f.Prepare().size() == 2 && !f.Selection.Ranges(0)[0].Bounds,
        "a projective model remains conservatively unknown");
  f.Rows[0].Current[3] = 0;
  f.Rows[0].Current[0] = std::numeric_limits<float>::quiet_NaN();
  CHECK(f.Prepare().size() == 2 && !f.Selection.Ranges(0)[0].Bounds,
        "nonfinite placements cannot eliminate geometry");
  f.Batches[0].FirstJob = 2;
  CHECK(f.Prepare().empty(), "job ranges outside the resident table remain unqualified");
}
}

int main() {
  Coverage();
  Bounds();
  return Report();
}
