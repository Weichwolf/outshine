#include "Check.h"
#include "HipRoofPlan.h"

#include <algorithm>
#include <array>
#include <cstddef>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  std::array<EastNorth, 4> rectangle{
      {{100000, -200000}, {100012, -199984}, {100004, -199978}, {99992, -199994}}};
  for (int winding = 0; winding < 2; ++winding) {
    const auto plan = HipRoofPlan::Prepare(rectangle, {}, {});
    CHECK(plan.has_value(), "an analytic rectangle preserves its rotated footprint");
    if (plan) {
      CHECK_NEAR(plan->MaximumDistanceM(), 5, 1e-9, "metres", "rectangle inradius");
      CHECK(plan->Facets().size() == 4, "all original eaves remain distinct owners");
    }
    std::ranges::reverse(rectangle);
  }
  const std::array<EastNorth, 3> triangle{{{0, 0}, {12, 0}, {0, 9}}};
  const auto complex = HipRoofPlan::Prepare(triangle, {}, {});
  CHECK(complex.has_value(), "the contour recipe builds a nonrectangular roof");
  if (complex) {
    CHECK_NEAR(complex->MaximumDistanceM(), 3, 1e-10, "metres", "triangle inradius");
    CHECK(complex->Facets().size() == 3, "every original triangular eave owns one facet");
  }
  return Report();
}
