#include "Check.h"
#include "HeightProgram.h"
#include "WaterHeightFit.h"
#include <array>
#include <cmath>
#include <cstdint>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

void CheckRoad() {
  using namespace outshine::Generators::RoadProfiles;
  std::array terms{SparseEntry<int>{0, 0, 1},
                   SparseEntry<int>{1, 0, -1},
                   SparseEntry<int>{2, 0, 1},
                   SparseEntry<int>{2, 1, 1},
                   SparseEntry<int>{3, 0, -1},
                   SparseEntry<int>{3, 1, -1},
                   SparseEntry<int>{4, 0, 1},
                   SparseEntry<int>{4, 1, 2},
                   SparseEntry<int>{4, 2, -1},
                   SparseEntry<int>{5, 0, -1},
                   SparseEntry<int>{5, 1, -2},
                   SparseEntry<int>{5, 2, -1},
                   SparseEntry<int>{6, 1, 1},
                   SparseEntry<int>{7, 1, -1}};
  const HeightProgram program{.Conditions = SparseMatrix<int>::Assemble(8, 3, terms),
                              .Upper = {10, -10, 10.1, -10.1, 11, -11, .2, .2},
                              .Variables = {{}, {}, {0, 2}},
                              .AdjustmentVariable = 2};
  const auto fit = FitHeights(program, {}, {});
  CHECK(fit.has_value(), "HiGHS solves the actual road programme beside OSQP");
  if (!fit) { return; }
  CHECK_NEAR(fit->Values[0], 10, 1e-9, "m", "the fixed road height remains exact");
  CHECK_NEAR(fit->Values[1], .1, 1e-9, "m/m", "the affine road slope remains exact");
  CHECK_NEAR(fit->MaximumAdjustmentM, .8, 1e-9, "m", "the minimax adjustment is unchanged");
  CHECK(fit->MaximumViolation <= 1e-9, "the original road conditions remain satisfied");
}

void CheckWater() {
  const std::array<std::array<double, 2>, 4> positions{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
  const std::array<WaterSurfaceMesh::Face, 2> faces{
      {{.Vertices = {0, 1, 2}}, {.Vertices = {0, 2, 3}}}};
  const std::array<double, 4> target{0, 1, 1, 0};
  const std::array<WaterGradeConstraint, 4> grades{
      {{.Face = 0, .DirectionEnu = {{1, 0, 0}}, .Lower = -.1, .Upper = 0},
       {.Face = 1, .DirectionEnu = {{1, 0, 0}}, .Lower = -.1, .Upper = 0},
       {.Face = 0, .DirectionEnu = {{0, 1, 0}}, .Lower = -.005, .Upper = .005},
       {.Face = 1, .DirectionEnu = {{0, 1, 0}}, .Lower = -.005, .Upper = .005}}};
  const std::array<std::array<uint32_t, 2>, 2> primary{{{0, 2}, {1, 3}}};
  const WaterHeightProblem problem{.Frame = TangentFrame::At({}),
                                   .VerticesXY = positions,
                                   .Faces = faces,
                                   .TargetUpM = target,
                                   .Grades = grades,
                                   .PrimaryGrades = primary,
                                   .UpBoundsM = {-1, 2},
                                   .Contacts = {}};
  const auto fit = FitWaterSurfaceHeights(problem);
  CHECK(fit.has_value(), "OSQP solves the actual water programme beside HiGHS");
  if (!fit) { return; }
  for (const auto &vertex : fit->VerticesEnuM) {
    CHECK_NEAR(vertex[2], .5, 1e-4, "m", "the analytic integrated water height is unchanged");
  }
  CHECK_NEAR(fit->IntegratedRmsM, std::sqrt(1. / 12), 1e-4, "m", "the physical RMS is unchanged");
  CHECK(fit->WholeFaceViolation <= 1e-9 && fit->DualGap <= 1e-7,
        "the original water constraints and optimality bound remain satisfied");
}
}

int main() {
  CheckRoad();
  CheckWater();
  CheckRoad();
  return Report();
}
