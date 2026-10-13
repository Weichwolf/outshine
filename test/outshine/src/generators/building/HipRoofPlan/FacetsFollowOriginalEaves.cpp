#include "Check.h"
#include "HipRoofPlan.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace {
using outshine::EastNorth;
using namespace outshine::Generators;
using namespace outshine::Test;
using Ring = std::vector<EastNorth>;

EastNorth Transformed(double x, double y) {
  return {.EastM = 100000 + 0.6 * x - 0.8 * y, .NorthM = -200000 + 0.8 * x + 0.6 * y};
}

void Rectangle() {
  const Ring ring{Transformed(0, 0), Transformed(20, 0), Transformed(20, 10), Transformed(0, 10)};
  const auto plan = HipRoofPlan::Prepare(ring, {}, {});
  CHECK(plan.has_value(), "rotated translated rectangle has a native roof partition");
  if (!plan) { return; }
  CHECK(plan->Facets().size() == 4, "each original eave owns one facet");
  CHECK_NEAR(
      plan->MaximumDistanceM(), 5, 1e-9, "wavefront metres", "rectangle extinction distance");
  for (int x = 0; x < 20; ++x) {
    for (int y = 0; y < 10; ++y) {
      const double u = x + 0.37, v = y + 0.29;
      const auto distance = plan->DistanceAt(Transformed(u, v));
      CHECK(distance.has_value(), "all interior positions are covered");
      if (distance) {
        CHECK_NEAR(*distance,
                   std::min({u, 20 - u, v, 10 - v}),
                   1e-9,
                   "wavefront metres",
                   "independent rectangle distance agrees after rigid transform");
      }
    }
  }
  for (const auto &point : ring) {
    const auto height = plan->DistanceAt(point);
    CHECK(height && std::abs(*height) < 1e-9, "actual eaves stay at zero roof rise");
  }
  CHECK(!plan->DistanceAt(Transformed(21, 5)), "outside positions are not silently extended");
}

void Nonrectangular() {
  const Ring triangle{{0, 0}, {12, 0}, {0, 9}};
  const auto triangular = HipRoofPlan::Prepare(triangle, {}, {});
  CHECK(triangular.has_value(), "triangle uses original eaves instead of its enclosing rectangle");
  if (triangular) {
    CHECK_NEAR(
        triangular->MaximumDistanceM(), 3, 1e-10, "wavefront metres", "right triangle inradius");
    const auto apex = triangular->DistanceAt({3, 3});
    CHECK(apex.has_value(), "triangle apex is shared by all native facets");
    if (apex) { CHECK_NEAR(*apex, 3, 1e-10, "wavefront metres", "triangle apex rise"); }
  }
  const Ring concave{{0, 0}, {10, 0}, {10, 4}, {4, 4}, {4, 10}, {0, 10}};
  const auto elbow = HipRoofPlan::Prepare(concave, {}, {});
  CHECK(elbow.has_value(), "concave footprint retains its actual partition");
  if (elbow) {
    CHECK(!elbow->DistanceAt({7, 7}), "the missing corner remains outside the roof");
    for (const auto &point : concave) {
      const auto distance = elbow->DistanceAt(point);
      CHECK(distance && std::abs(*distance) < 1e-10, "concave eaves stay level");
    }
  }
}

void AxisAligned() {
  for (const auto dimensions :
       std::array<std::array<double, 2>, 3>{{{20, 10}, {10, 20}, {10, 10}}}) {
    const double width = dimensions[0];
    const double depth = dimensions[1];
    Ring ring{{0, 0}, {width, 0}, {width, depth}, {0, depth}};
    for (int winding = 0; winding < 2; ++winding) {
      const auto plan = HipRoofPlan::Prepare(ring, {}, {});
      CHECK(plan.has_value(), "analytic long/short/square rectangles support either winding");
      if (plan) {
        CHECK(plan->Facets().size() == 4, "rectangle specialization preserves four source facets");
        CHECK_NEAR(plan->MaximumDistanceM(),
                   std::min(width, depth) / 2,
                   1e-10,
                   "wavefront metres",
                   "analytic radius agrees with the inradius");
        for (int x = 0; x < static_cast<int>(width); ++x) {
          for (int y = 0; y < static_cast<int>(depth); ++y) {
            const double u = x + 0.37;
            const double v = y + 0.29;
            const auto distance = plan->DistanceAt({u, v});
            CHECK(distance.has_value(), "analytic facets cover every interior sample");
            if (distance) {
              CHECK_NEAR(*distance,
                         std::min({u, width - u, v, depth - v}),
                         1e-10,
                         "wavefront metres",
                         "rectangle travel follows all four actual eaves");
            }
          }
        }
      }
      std::ranges::reverse(ring);
    }
  }
}

void Courtyard() {
  Ring outer{{0, 0}, {20, 0}, {20, 20}, {0, 20}};
  std::array<Ring, 1> holes{Ring{{8, 8}, {8, 12}, {12, 12}, {12, 8}}};
  for (int winding = 0; winding < 2; ++winding) {
    const auto plan = HipRoofPlan::Prepare(outer, holes, {});
    CHECK(plan.has_value(), "both input windings preserve courtyard topology");
    if (plan) {
      CHECK(!plan->DistanceAt({10, 10}), "courtyard remains open");
      const auto beside = plan->DistanceAt({5, 10});
      CHECK(beside.has_value(), "roof beside courtyard is covered");
      if (beside) {
        CHECK_NEAR(*beside, 3, 1e-10, "wavefront metres", "inner eave controls this slope");
      }
      for (const auto &point : holes[0]) {
        const auto distance = plan->DistanceAt(point);
        CHECK(distance && std::abs(*distance) < 1e-10, "courtyard eaves stay level");
      }
    }
    std::ranges::reverse(outer);
    std::ranges::reverse(holes[0]);
  }
}

void Refusals() {
  Ring ring{{0, 0}, {20, 0}, {20, 10}, {0, 10}};
  for (const auto limit : {HipRoofPlan::Limits{.Vertices = 3},
                           HipRoofPlan::Limits{.Nodes = 1},
                           HipRoofPlan::Limits{.Facets = 1},
                           HipRoofPlan::Limits{.Corners = 3}}) {
    const auto plan = HipRoofPlan::Prepare(ring, {}, limit);
    CHECK(!plan && plan.error() == HipRoofError::CapacityExceeded,
          "each explicit output/input capacity refuses the entire plan");
  }
  ring[1].EastM = std::numeric_limits<double>::quiet_NaN();
  const auto invalid = HipRoofPlan::Prepare(ring, {}, {});
  CHECK(!invalid && invalid.error() == HipRoofError::InvalidInput,
        "nonfinite input is refused before library calls");
}
}

int main() {
  Rectangle();
  Nonrectangular();
  AxisAligned();
  Courtyard();
  Refusals();
  return Report();
}
