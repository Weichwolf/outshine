#include "StructureSurfaceRefinement.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>

namespace {
using namespace outshine;
using namespace outshine::Generators;

Raised Grid(float height) {
  Raised mesh;
  for (int y = 0; y < 48; ++y) {
    for (int x = 0; x < 48; ++x) {
      const float left = static_cast<float>(x);
      const float bottom = static_cast<float>(y);
      const std::array<Vec3f, 6> points{{{{left, bottom, height}},
                                         {{left + 1, bottom, height}},
                                         {{left + 1, bottom + 1, height}},
                                         {{left, bottom, height}},
                                         {{left + 1, bottom + 1, height}},
                                         {{left, bottom + 1, height}}}};
      for (const auto point : points) {
        mesh.WallRun.push_back(static_cast<uint32_t>(mesh.WallCorners.size()));
        mesh.WallCorners.push_back({.pos = point});
      }
    }
  }
  return mesh;
}
}

int main() {
  using namespace outshine::Test;
  const auto reference = Grid(0);
  for (const float distance : {0.0f, 2.0f, 5.0f}) {
    auto variant = Grid(distance == 5 ? 0 : distance);
    if (distance == 5) { variant.WallCorners.back().pos[2] = 5; }
    for (const bool reverse : {false, true}) {
      if (reverse) { std::ranges::reverse(variant.WallRun); }
      StructureSurfaceRefinementTask task;
      CHECK(task.Reset({reference, variant, 91}), "dense native planes start under unchanged caps");
      bool complete = false;
      bool safe = true;
      bool paced = true;
      for (size_t slice = 0; slice < 50000; ++slice) {
        const size_t budget = slice % 3 == 0 ? 1 : 128;
        const size_t work = task.WorkUnits();
        const auto result = task.Step(91, {.MaxWorkUnits = budget});
        paced &= task.WorkUnits() - work <= budget;
        if (!result) {
          safe = false;
          break;
        }
        if (const auto bound = task.Bound(91)) {
          safe &= bound->LowerDistanceM() <= distance && distance <= bound->UpperDistanceM();
        }
        if (*result == StructureSurfaceErrorProgress::Complete) {
          complete = true;
          break;
        }
      }
      const auto bound = task.Bound(91);
      CHECK(complete && safe && paced, "every partial certificate covers the analytic distance");
      if (bound && bound->UpperDistanceM() - bound->LowerDistanceM() > 0.02) {
        std::printf("dense distance=%g reverse=%d lower=%g upper=%g queries=%zu work=%zu "
                    "regions=%zu splits=%zu\n",
                    static_cast<double>(distance),
                    reverse,
                    bound->LowerDistanceM(),
                    bound->UpperDistanceM(),
                    task.TriangleQueries(),
                    task.WorkUnits(),
                    task.RegionCount(),
                    task.Splits());
      }
      CHECK(bound && bound->UpperDistanceM() - bound->LowerDistanceM() <= 0.02,
            "more than 4096 triangles obtain a useful interval without increasing caps");
      CHECK(task.RegionCount() <= 4096 && task.TriangleQueries() <= 131072,
            "hierarchy respects live-region and primitive-query bounds");
    }
  }
  Raised point;
  point.WallCorners.push_back({.pos = {{0, 0, 0}}});
  point.WallRun = {0, 0, 0};
  StructureSurfaceRefinementTask exact;
  CHECK(exact.Reset({point, point, 93}, {.TargetUncertaintyM = 0}), "exact self proof starts");
  const auto done = exact.Step(93, {.MaxWorkUnits = 128});
  CHECK(done && *done == StructureSurfaceErrorProgress::Complete && exact.Bound(93) &&
            exact.Bound(93)->UpperDistanceM() == 0,
        "empty frontier completes even at zero tolerance");
  return Report();
}
