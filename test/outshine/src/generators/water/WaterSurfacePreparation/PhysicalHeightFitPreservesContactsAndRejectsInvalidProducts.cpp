#include "WaterHeightFit.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stop_token>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

struct Case {
  TangentFrame Frame = TangentFrame::At({});
  std::array<std::array<double, 2>, 4> XY{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
  std::array<WaterSurfaceMesh::Face, 2> Faces{{{.Vertices = {0, 1, 2}}, {.Vertices = {0, 2, 3}}}};
  std::array<double, 4> Target{0, 1, 1, 0};
  std::array<WaterGradeConstraint, 4> Grades{
      {{.Face = 0, .DirectionEnu = {{1, 0, 0}}, .Lower = -.1, .Upper = 0},
       {.Face = 1, .DirectionEnu = {{1, 0, 0}}, .Lower = -.1, .Upper = 0},
       {.Face = 0, .DirectionEnu = {{0, 1, 0}}, .Lower = -.005, .Upper = .005},
       {.Face = 1, .DirectionEnu = {{0, 1, 0}}, .Lower = -.005, .Upper = .005}}};
  std::array<std::array<uint32_t, 2>, 2> Primary{{{0, 2}, {1, 3}}};

  WaterHeightProblem Problem(std::span<const WaterHeightContact> contacts = {}) const {
    return {.Frame = Frame,
            .VerticesXY = XY,
            .Faces = Faces,
            .TargetUpM = Target,
            .Grades = Grades,
            .PrimaryGrades = Primary,
            .UpBoundsM = {-1, 2},
            .Contacts = contacts};
  }
};

void CheckIntegratedObjective() {
  const Case source;
  const auto fit = FitWaterSurfaceHeights(source.Problem());
  if (!fit) { std::cerr << "integrated fit error " << static_cast<int>(fit.error()) << '\n'; }
  CHECK(fit, "native exact-area fitting returns a physically checked shared surface");
  if (!fit) { return; }
  for (const auto &vertex : fit->VerticesEnuM) {
    CHECK(std::abs(vertex[2] - .5) < 1e-4,
          "exact integration avoids the artificial cross-slope of lumped vertex weights");
  }
  CHECK(std::abs(fit->IntegratedRmsM - std::sqrt(1. / 12)) < 1e-4,
        "the area integral agrees with the analytic square rather than a vertex average");
  CHECK(fit->WholeFaceViolation <= 1e-9 && fit->DualGap <= 1e-7 && fit->Rows > 0 &&
            fit->MatrixBytes > 0 && fit->Iterations > 0,
        "a solver status is accompanied by physical, optimality and work evidence");
}

void CheckContacts() {
  Case source;
  source.Target.fill(.5);
  for (auto &grade : source.Grades) {
    grade.Lower = -.1;
    grade.Upper = .1;
  }
  const std::array<WaterHeightContact, 1> point{
      {{.Face = 0, .Barycentric = {.5, .25, .25}, .UpM = .55}}};
  const auto fit = FitWaterSurfaceHeights(source.Problem(point));
  if (!fit) { std::cerr << "contact fit error " << static_cast<int>(fit.error()) << '\n'; }
  CHECK(fit && fit->MaximumContactErrorM <= 1e-9,
        "an interior contact constrains the same native facets used for rendering");
  const std::array<WaterHeightContact, 2> conflict{
      {point[0], {.Face = 0, .Barycentric = {.5, .25, .25}, .UpM = .65}}};
  const auto refused = FitWaterSurfaceHeights(source.Problem(conflict));
  if (!refused && refused.error() != WaterHeightError::Infeasible) {
    std::cerr << "conflict fit error " << static_cast<int>(refused.error()) << '\n';
  }
  CHECK(!refused && refused.error() == WaterHeightError::Infeasible,
        "incompatible physical contacts publish no relaxed or partial height field");
}

void CheckBudgetsAndInput() {
  Case source;
  std::stop_source stopped;
  stopped.request_stop();
  const auto canceled = FitWaterSurfaceHeights(source.Problem(), {}, stopped.get_token());
  CHECK(!canceled && canceled.error() == WaterHeightError::Canceled,
        "canceled jobs cannot allocate a solver or publish geometry");
  for (const auto budget : {WaterHeightBudget{.Vertices = 1},
                            WaterHeightBudget{.Faces = 1},
                            WaterHeightBudget{.Rows = 1},
                            WaterHeightBudget{.Iterations = 0},
                            WaterHeightBudget{.SolveSeconds = 0}}) {
    const auto fit = FitWaterSurfaceHeights(source.Problem(), budget);
    CHECK(!fit && fit.error() == WaterHeightError::CapacityExceeded,
          "explicit local construction and solving budgets are enforced before solving");
  }
  source.Grades[2].DirectionEnu = source.Grades[0].DirectionEnu;
  CHECK(!FitWaterSurfaceHeights(source.Problem()),
        "parallel primary directions cannot manufacture a finite normal bound");
  source.Primary[0][0] = 100;
  const auto invalid = FitWaterSurfaceHeights(source.Problem());
  CHECK(!invalid && invalid.error() == WaterHeightError::InvalidInput,
        "unbound primary rows are rejected at the model boundary");
}
}

int main() {
  CheckIntegratedObjective();
  CheckContacts();
  CheckBudgetsAndInput();
  return Report();
}
