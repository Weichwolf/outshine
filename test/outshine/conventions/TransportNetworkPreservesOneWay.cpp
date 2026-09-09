#include "Wayfinding.h"
#include "Check.h"
#include <array>
#include <string>

namespace {
void CheckSplice(bool reverseRoad, bool outwardSpur, bool oneWay) {
  using namespace outshine;
  using namespace outshine::Test;
  const LongitudeLatitude west{.LongitudeDeg = 0, .LatitudeDeg = 0};
  const LongitudeLatitude east{.LongitudeDeg = 0.02, .LatitudeDeg = 0};
  const LongitudeLatitude junction{.LongitudeDeg = 0.01, .LatitudeDeg = 0};
  const LongitudeLatitude south{.LongitudeDeg = 0.01, .LatitudeDeg = -0.01};
  const std::array<double, 4> road =
      reverseRoad ? std::array<double, 4>{0, 0.02, 0, 0} : std::array<double, 4>{0, 0, 0, 0.02};
  const std::array<double, 4> spur = outwardSpur ? std::array<double, 4>{0, 0.01, -0.01, 0.01}
                                                 : std::array<double, 4>{-0.01, 0.01, 0, 0.01};
  const std::array<double, 4> anchor{-0.01, 0.02, 0, 0.02};
  Path::Network network({.CellM = 1}, {});
  network.Lay(road, {.HalfWidthM = 1, .Oneway = oneWay});
  network.Lay(spur, {.HalfWidthM = 1, .Oneway = true});
  network.Lay(anchor, {.HalfWidthM = 1, .Oneway = true});
  std::string error;
  CHECK(network.Weave(error), "directed branch graph builds");
  CHECK(network.TiedToEdges() == 1, "physical loose endpoint splices even with no outgoing edge");
  CHECK(network.EdgeCount() == (oneWay ? 4u : 6u),
        "split retains exactly the permitted directed arcs");
  const auto start = reverseRoad ? east : west;
  const auto finish = reverseRoad ? west : east;
  CHECK(network.Plan(start, finish, 0).Found, "split preserves forward through route");
  CHECK(network.Plan(finish, start, 0).Found == !oneWay,
        "split cannot invent reverse through route");
  CHECK(network.Plan(junction, finish, 0).Found, "junction follows main road direction");
  CHECK(network.Plan(south, finish, 0).Found == !outwardSpur,
        "entering from spur respects its direction");
  CHECK(network.Plan(start, south, 0).Found == outwardSpur,
        "leaving into spur respects its direction");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array<double, 4> points{0, 0, 0, 0.01};
  const LongitudeLatitude west{.LongitudeDeg = 0, .LatitudeDeg = 0};
  const LongitudeLatitude east{.LongitudeDeg = 0.01, .LatitudeDeg = 0};
  for (const bool oneWay : {false, true}) {
    Path::Network network({.CellM = 1}, {});
    network.Lay(points, {.Oneway = oneWay});
    std::string error;
    CHECK(network.Weave(error), "two-node transport network builds");
    CHECK(network.Plan(west, east, 0).Found, "forward travel is permitted");
    CHECK(network.Plan(east, west, 0).Found == !oneWay,
          "reverse travel exists exactly when the way permits it");
  }
  for (const bool reverse : {false, true}) {
    for (const bool outward : {false, true}) {
      for (const bool oneWay : {false, true}) { CheckSplice(reverse, outward, oneWay); }
    }
  }
  return Report();
}
