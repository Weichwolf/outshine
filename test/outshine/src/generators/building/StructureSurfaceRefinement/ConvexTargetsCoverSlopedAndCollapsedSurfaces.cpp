#include "StructureSurfaceRefinement.h"
#include "Check.h"
#include <array>
#include <cstdint>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;

Raised Surface(std::array<Vec3f, 3> vertices, Vec3f offset, bool reverse) {
  Raised mesh;
  for (const auto &vertex : vertices) { mesh.RoofCorners.push_back({.pos = vertex + offset}); }
  mesh.RoofRun = reverse ? std::vector<uint32_t>{2, 1, 0} : std::vector<uint32_t>{0, 1, 2};
  return mesh;
}
}

int main() {
  using namespace outshine::Test;
  const std::array shapes{std::array<Vec3f, 3>{{{{0, 0, 0}}, {{6, 0, 0}}, {{0, 8, 6}}}},
                          std::array<Vec3f, 3>{{{{0, 0, 0}}, {{6, 0, 0}}, {{3, 0, 0}}}},
                          std::array<Vec3f, 3>{{{{0, 0, 0}}, {{0, 0, 0}}, {{0, 0, 0}}}}};
  for (const auto &shape : shapes) {
    for (const float anchor : {0.0f, 1048576.0f}) {
      for (const bool reverse : {false, true}) {
        for (const float distance : {0.0f, 5.0f}) {
          const Vec3f origin{{anchor, -anchor, anchor}};
          const Vec3f offset = distance == 0 ? Vec3f{} : Vec3f{{0, -3, 4}};
          const Raised reference = Surface(shape, origin, false);
          const Raised variant = Surface(shape, origin + offset, reverse);
          for (size_t budget = 0; budget <= 8; ++budget) {
            StructureSurfaceRefinementTask task;
            CHECK(task.Reset({reference, variant, 41}, {.MaxTriangleQueries = budget}),
                  "analytic convex pair starts with bounded primitive queries");
            bool finished = false;
            bool safe = true;
            for (size_t step = 0; step < 100; ++step) {
              const size_t before = task.TriangleQueries();
              const auto progress = task.Step(41, {.MaxWorkUnits = 1});
              safe &= progress.has_value() && task.TriangleQueries() - before <= 1 &&
                      task.TriangleQueries() <= budget;
              if (const auto bound = task.Bound(41)) {
                safe &= bound->LowerDistanceM() <= distance && distance <= bound->UpperDistanceM();
              }
              if (!progress || *progress == StructureSurfaceErrorProgress::Complete) {
                finished = progress.has_value();
                break;
              }
            }
            const auto bound = task.Bound(41);
            CHECK(finished && safe && bound,
                  "every exposed bound contains the analytic normal translation under partial "
                  "budgets");
            if (budget == 8) {
              CHECK(bound && bound->UpperDistanceM() <= distance + 0.02 && task.Splits() == 0,
                    "fixed convex targets bound sloped and degenerate surfaces without "
                    "subdivision");
            }
          }
        }
      }
    }
  }
  return Report();
}
