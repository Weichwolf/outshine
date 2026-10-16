#include "JunctionSlopePlan.h"
#include "Check.h"
#include "Json.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Generators;
using namespace outshine::Test;

void ActualWienFailure() {
  std::ifstream file("test/outshine/data/transport/wien-junction-slopes.json");
  const std::string text(std::istreambuf_iterator<char>{file}, {});
  outshine::Json fixture;
  const bool parsed = fixture.Parse(text.data(), text.size());
  CHECK(parsed, "the actual offline Wien solver operands are available");
  if (!parsed) { return; }
  std::vector<JunctionSlope> slopes;
  const auto sourceSlopes = fixture.Root()["slopes"];
  for (size_t i = 0; i < sourceSlopes.Size(); ++i) {
    const auto row = sourceSlopes[i];
    slopes.push_back(
        {.East = row[size_t{0}].Num(), .North = row[1].Num(), .Maximum = row[2].Num()});
  }
  std::vector<JunctionSlopeConstraint> constraints;
  const auto sourceConstraints = fixture.Root()["constraints"];
  for (size_t i = 0; i < sourceConstraints.Size(); ++i) {
    const auto row = sourceConstraints[i];
    JunctionSlopeConstraint constraint{.Terms = {}, .MinimumRiseM = row["minimum_rise_m"].Num()};
    const auto terms = row["terms"];
    for (size_t j = 0; j < terms.Size(); ++j) {
      const auto term = terms[j];
      constraint.Terms.push_back({.Junction = static_cast<uint32_t>(term[size_t{0}].Int()),
                                  .EastM = term[1].Num(),
                                  .NorthM = term[2].Num()});
    }
    constraints.push_back(std::move(constraint));
  }
  const auto result = FitJunctionSlopes(
      slopes, constraints, std::chrono::steady_clock::now() + std::chrono::seconds(1));
  CHECK(!result && result.error().Kind == JunctionSlopeRefusal::NoRoom &&
            result.error().Constraint == 0,
        "the real conflict has a capacity witness rather than exhausted projection work");
  if (result) { return; }
  constexpr double expectedCapacityM = 0.8395873739748207;
  constexpr double roundoffM = 1e-12;
  CHECK(std::abs(result.error().MaximumRiseM - expectedCapacityM) < roundoffM,
        "the independent exact disk-support bound is retained in metres");
  for (const auto &slope : slopes) {
    CHECK(slope.East == 0 && slope.North == 0,
          "a proven impossible request does not publish partially projected slopes");
  }
  double attainableM = 0;
  for (const auto &term : constraints.front().Terms) {
    const double lengthM = std::sqrt(term.EastM * term.EastM + term.NorthM * term.NorthM);
    const double scale = slopes[term.Junction].Maximum / lengthM;
    attainableM += term.EastM * (term.EastM * scale) + term.NorthM * (term.NorthM * scale);
  }
  CHECK(std::abs(attainableM - result.error().MaximumRiseM) < roundoffM &&
            attainableM < constraints.front().MinimumRiseM,
        "aligned maximum legal slopes attain the bound and still cannot supply the required rise");
}

void UnresolvedIsNotAProof() {
  std::array slopes{JunctionSlope{.East = 0, .North = 0, .Maximum = 1}};
  const std::array constraints{
      JunctionSlopeConstraint{.Terms = {{.Junction = 0, .EastM = 1, .NorthM = 0}},
                              .MinimumRiseM = .5},
      JunctionSlopeConstraint{.Terms = {{.Junction = 0, .EastM = -1, .NorthM = 0}},
                              .MinimumRiseM = .5}};
  const auto result = FitJunctionSlopes(
      slopes, constraints, std::chrono::steady_clock::now() + std::chrono::seconds(1));
  CHECK(!result && result.error().Kind == JunctionSlopeRefusal::ProjectionBound &&
            !result.error().Constraint,
        "individually attainable conflicting requests remain an unresolved bounded solve");
  const auto expired = FitJunctionSlopes(slopes, constraints, std::chrono::steady_clock::now());
  CHECK(!expired && expired.error().Kind == JunctionSlopeRefusal::Deadline &&
            !expired.error().Constraint,
        "an exhausted deadline does not claim to prove geometric infeasibility");
}

}

int main() {
  ActualWienFailure();
  UnresolvedIsNotAProof();
  return Report();
}
