#include "RoadPortFieldFit.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stop_token>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr double kLengthM = 100;
constexpr double kHeightM = 12;
constexpr double kRiseM = 1;
constexpr double kPriorChangeM = .1;
constexpr double kGrade = .05;
constexpr double kCrossfall = .025;
constexpr double kAcrossGrade = .005;
constexpr double kTolerance = 1e-7;

struct Fixture {
  std::array<RoadPortField::Plane, 3> Planes{{{.Origin = {}, .Gradient = {}},
                                              {.Origin = {.EastM = kLengthM / 2}, .Gradient = {}},
                                              {.Origin = {.EastM = kLengthM}, .Gradient = {}}}};
  std::array<RoadPortField::Patch, 5> Patches{
      {{.Parents = {0, 0}, .CutCentres = {}, .InwardNormals = {}},
       {.Parents = {0, 1},
        .CutCentres = {{{.EastM = 2}, {.EastM = 48}}},
        .InwardNormals = {{{.EastM = 1}, {.EastM = -1}}},
        .ScaleM = 46},
       {.Parents = {1, 1}, .CutCentres = {}, .InwardNormals = {}},
       {.Parents = {1, 2},
        .CutCentres = {{{.EastM = 52}, {.EastM = 98}}},
        .InwardNormals = {{{.EastM = 1}, {.EastM = -1}}},
        .ScaleM = 46},
       {.Parents = {2, 2}, .CutCentres = {}, .InwardNormals = {}}}};
  std::vector<RoadPortPrior> Priors;
  std::vector<RoadPortFace> Faces;

  Fixture() {
    constexpr std::array stations{0., 2., 48., 50., 52., 98., kLengthM};
    constexpr std::array<uint32_t, 6> owners{0, 1, 2, 2, 3, 4};
    for (size_t at = 0; at < stations.size(); ++at) {
      const double x = stations[at];
      for (const double y : std::array{-1., 1.}) {
        Priors.push_back({.Patch = owners[std::min(at, owners.size() - 1)],
                          .At = {.EastM = x, .NorthM = y},
                          .HeightM = kHeightM +
                                     kRiseM * std::min(x, kLengthM - x) / (kLengthM / 2) +
                                     kAcrossGrade * y});
      }
      if (at + 1 == stations.size()) { continue; }
      const double next = stations[at + 1];
      const EastNorth a{.EastM = x, .NorthM = -1};
      const EastNorth b{.EastM = x, .NorthM = 1};
      const EastNorth c{.EastM = next, .NorthM = -1};
      const EastNorth d{.EastM = next, .NorthM = 1};
      Faces.push_back({.Patch = owners[at],
                       .Triangle = {a, c, b},
                       .GradeFrame = {1 / kGrade, 0, 0, 1 / kCrossfall}});
      Faces.push_back({.Patch = owners[at],
                       .Triangle = {c, d, b},
                       .GradeFrame = {1 / kGrade, 0, 0, 1 / kCrossfall}});
    }
  }

  [[nodiscard]] RoadPortFieldFitInput Input() const {
    return {.Planes = Planes,
            .Patches = Patches,
            .Priors = Priors,
            .Faces = Faces,
            .MaximumPriorChangeM = kPriorChangeM};
  }
};

void CheckFit(const Fixture &fixture) {
  const auto fitted = FitRoadPortField(fixture.Input(), {}, {});
  CHECK(fitted.has_value(), "a bounded smooth field fits the non-affine full-width terrain");
  if (!fitted) { return; }
  CHECK(fitted->MaximumPriorErrorM <= kPriorChangeM + kTolerance &&
            fitted->MaximumNormalizedGrade <= 1,
        "the original terrain prior and whole-face gradient limits hold together");
  const auto left = fitted->Field.Evaluate(0, {});
  const auto middle = fitted->Field.Evaluate(2, {.EastM = kLengthM / 2});
  const auto right = fitted->Field.Evaluate(4, {.EastM = kLengthM});
  CHECK(left && middle && right &&
            middle->HeightM - (left->HeightM + right->HeightM) / 2 >=
                kRiseM - 2 * kPriorChangeM - kTolerance,
        "an affine replacement cannot satisfy the accepted non-affine terrain inputs");
  constexpr std::array cuts{2., 48., 52., 98.};
  for (size_t at = 0; at < cuts.size(); ++at) {
    const auto a = fitted->Field.Evaluate(at, {.EastM = cuts[at], .NorthM = 1});
    const auto b = fitted->Field.Evaluate(at + 1, {.EastM = cuts[at], .NorthM = 1});
    CHECK(a && b && std::abs(a->HeightM - b->HeightM) <= kTolerance &&
              std::abs(a->Gradient.SlopeE - b->Gradient.SlopeE) <= kTolerance &&
              std::abs(a->Gradient.SlopeN - b->Gradient.SlopeN) <= kTolerance,
          "both sides retain height and the entire tangent plane at each shared cut");
  }
  CHECK(fitted->Work.Observations == fixture.Priors.size() &&
            fitted->Work.Faces == fixture.Faces.size() &&
            fitted->Work.Conditions > 2 * fixture.Priors.size(),
        "the field reports actual original observations and continuous constraints");
  Note("native smooth port field solve", fitted->Solver.SolveMs, "ms");
  Note("native smooth port field prior error", fitted->MaximumPriorErrorM, "m");
}

void CheckFailures(const Fixture &fixture) {
  auto budget = RoadProfiles::HeightFitBudget{};
  budget.Variables = 1;
  const auto refused = FitRoadPortField(fixture.Input(), budget);
  CHECK(!refused && refused.error() == RoadProfiles::HeightFitError::CapacityExceeded,
        "inadequate variable admission cannot enter the solver");
  std::stop_source stopped;
  stopped.request_stop();
  const auto canceled = FitRoadPortField(fixture.Input(), {}, stopped.get_token());
  CHECK(!canceled && canceled.error() == RoadProfiles::HeightFitError::Canceled,
        "cancellation remains explicit before source field preparation");
  auto malformed = fixture.Priors;
  malformed[0].HeightM = std::numeric_limits<double>::quiet_NaN();
  auto input = fixture.Input();
  input.Priors = malformed;
  const auto invalid = FitRoadPortField(input, {});
  CHECK(!invalid && invalid.error() == RoadProfiles::HeightFitError::InvalidInput,
        "missing terrain observations cannot become invented zero heights");
}
}

int main() {
  const Fixture fixture;
  CheckFit(fixture);
  CheckFailures(fixture);
  return Report();
}
