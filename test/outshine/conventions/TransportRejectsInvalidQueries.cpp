#include "Wayfinding.h"
#include "Check.h"
#include <array>
#include <limits>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Path::Network network({.CellM = 1}, {});
  const std::array<double, 4> line{0, 0, 0, 0.01};
  network.Lay(line, {});
  std::string error;
  CHECK(network.Weave(error), "valid network builds");
  const LongitudeLatitude start{.LongitudeDeg = 0, .LatitudeDeg = 0};
  const LongitudeLatitude finish{.LongitudeDeg = 0.01, .LatitudeDeg = 0};
  for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity(),
                               181.0,
                               -181.0}) {
    for (const bool latitude : {false, true}) {
      auto point = start;
      if (latitude) {
        point.LatitudeDeg = invalid;
      } else {
        point.LongitudeDeg = invalid;
      }
      for (const bool invalidStart : {false, true}) {
        const auto route =
            network.Plan(invalidStart ? point : start, invalidStart ? finish : point, 0);
        CHECK(!route.Found && route.Legs.empty() && route.StraightM == 0 &&
                  route.Error.find("route coordinates") != std::string::npos,
              "invalid endpoint refused before distance calculation or spatial lookup");
      }
    }
  }
  for (const double latitude : {-91.0, 91.0}) {
    CHECK(!network.Plan({.LongitudeDeg = 0, .LatitudeDeg = latitude}, finish, 0).Found,
          "latitude has its own narrower domain");
  }
  for (const double radius : {-1.0,
                              std::numeric_limits<double>::quiet_NaN(),
                              std::numeric_limits<double>::infinity(),
                              -std::numeric_limits<double>::infinity()}) {
    const auto route = network.Plan(start, finish, radius);
    CHECK(!route.Found && route.Legs.empty() && route.StraightM == 0 &&
              route.Error.find("turn radius") != std::string::npos,
          "invalid turn radius is not silently treated as unrestricted");
  }
  CHECK(network.Plan(start, finish, 0).Found && network.Plan(start, finish, 1).Found,
        "valid zero and positive radius queries remain usable after failures");
  return Report();
}
