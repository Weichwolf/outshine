#include "HeightProgram.h"
#include "Check.h"

#include <array>
#include <vector>

namespace {

outshine::Generators::RoadProfiles::HeightProgram Program() {
  using namespace outshine;
  using namespace outshine::Generators::RoadProfiles;
  std::vector<SparseEntry<int>> terms{{0, 0, 1},
                                      {1, 0, -1},
                                      {2, 0, 1},
                                      {2, 1, 1},
                                      {3, 0, -1},
                                      {3, 1, -1},
                                      {4, 0, 1},
                                      {4, 1, 2},
                                      {4, 2, -1},
                                      {5, 0, -1},
                                      {5, 1, -2},
                                      {5, 2, -1},
                                      {6, 1, 1},
                                      {7, 1, -1}};
  return {.Conditions = SparseMatrix<int>::Assemble(8, 3, terms),
          .Upper = {10, -10, 10.1, -10.1, 11, -11, .2, .2},
          .Variables = {{}, {}, {0, 2}},
          .AdjustmentVariable = 2};
}

void AffinePortsAndInteriorTargetsUseTheSameProfile() {
  using namespace outshine::Generators::RoadProfiles;
  using namespace outshine::Test;
  auto program = Program();
  const auto fit = FitHeights(program, {}, {});
  CHECK(fit.has_value(), "a shared affine profile can retain two fixed rim heights");
  if (!fit) { return; }
  CHECK_NEAR(fit->Values[0], 10, 1e-9, "m", "the first fixed rim remains in place");
  CHECK_NEAR(fit->Values[1], .1, 1e-9, "m/m", "the whole transverse profile retains its slope");
  CHECK_NEAR(
      fit->MaximumAdjustmentM, .8, 1e-9, "m", "the analytic unavoidable prior error is attained");
  CHECK(fit->MaximumViolation <= 1e-9, "the solution is verified against the original conditions");
  program.Variables.back().Maximum = .79;
  const auto impossible = FitHeights(program, {}, {});
  CHECK(!impossible && impossible.error() == HeightFitError::Infeasible,
        "an insufficient prior radius refuses this local plan instead of moving fixed ports");
}

void DiscardedLibraryCoefficientsCannotBypassOriginalConditions() {
  using namespace outshine;
  using namespace outshine::Generators::RoadProfiles;
  using namespace outshine::Test;
  std::array terms{SparseEntry<int>{0, 0, 1e-10}};
  const HeightProgram program{.Conditions = SparseMatrix<int>::Assemble(1, 2, terms),
                              .Upper = {0},
                              .Variables = {{1e9, 1e9}, {0, 1}},
                              .AdjustmentVariable = 1};
  const auto fit = FitHeights(program, {}, {});
  CHECK(!fit && fit.error() == HeightFitError::VerificationFailed,
        "a dropped small coefficient still participates in original-matrix verification");
}

}

int main() {
  AffinePortsAndInteriorTargetsUseTheSameProfile();
  DiscardedLibraryCoefficientsCannotBypassOriginalConditions();
  return outshine::Test::Report();
}
