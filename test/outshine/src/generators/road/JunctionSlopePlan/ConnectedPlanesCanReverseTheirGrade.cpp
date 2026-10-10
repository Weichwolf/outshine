#include "JunctionSlopePlan.h"
#include "Check.h"

#include <array>
#include <chrono>
#include <cmath>
#include <vector>

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const auto deadline = [] { return std::chrono::steady_clock::now() + std::chrono::seconds(1); };
  std::array slopes{JunctionSlope{-.04, .03, .1}, JunctionSlope{.02, 0, .08}};
  const std::array constraints{JunctionSlopeConstraint{{{0, 8, 0}, {1, -4, 0}}, .5},
                               JunctionSlopeConstraint{{{0, 0, 5}, {1, 0, 3}}, .2}};
  CHECK(FitJunctionSlopes(slopes, constraints, deadline()).has_value(),
        "shared junction planes resolve both longitudinal and lateral attachment constraints");
  CHECK(slopes[0].East > 0, "a junction can reverse its initial DEM-derived grade");
  for (const auto &slope : slopes) {
    CHECK(std::hypot(slope.East, slope.North) <= slope.Maximum + 1e-12,
          "fitting preserves the road-class gradient bound");
  }
  for (const auto &constraint : constraints) {
    double riseM = 0;
    for (const auto &term : constraint.Terms) {
      riseM += term.EastM * slopes[term.Junction].East + term.NorthM * slopes[term.Junction].North;
    }
    CHECK(riseM >= constraint.MinimumRiseM - 1e-9,
          "every previously established attachment constraint remains satisfied");
  }
  const std::array impossible{JunctionSlopeConstraint{{{0, 1, 0}}, .11}};
  CHECK(!FitJunctionSlopes(slopes, impossible, deadline()),
        "an impossible ramp cannot raise the allowed junction slope");
  const std::array noRoom{JunctionSlopeConstraint{{}, .1}};
  CHECK(!FitJunctionSlopes(slopes, noRoom, deadline()),
        "a conflict without slope freedom is reported rather than discarded");
  return Report();
}
