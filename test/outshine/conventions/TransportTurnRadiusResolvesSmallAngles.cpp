#include "Wayfinding.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <numbers>
#include <limits>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr double longitudeStep = 0.02;
  constexpr double segmentM = kEarthMeanRadiusM * longitudeStep * std::numbers::pi / 180.0;
  for (const double sign : {-1.0, 1.0}) {
    for (const bool tiny : {false, true}) {
      auto created = Path::Network::Create({.CellM = 1}, {});
      CHECK(created.has_value(), "valid grid");
      if (!created) { return Report(); }
      auto &network = *created;
      const double endLat = sign * longitudeStep * (tiny ? 1e-8 : 1.0);
      const double endLon = tiny ? 2.0 * longitudeStep : longitudeStep;
      const std::array<double, 6> points{0, 0, 0, longitudeStep, endLat, endLon};
      CHECK(network.Lay(points, {.Oneway = true}).has_value(), "directed corner accepted");
      std::string error;
      CHECK(network.Weave(error) && network.NodeCount() == 3 && network.EdgeCount() == 2,
            "isolated two-edge corner builds without extra connections");
      const LongitudeLatitude finish{.LongitudeDeg = endLon, .LatitudeDeg = endLat};
      // At 90 degrees, tangent setback equals radius. For the tiny angle,
      // tan(theta/2) = slope/(sqrt(1+slope^2)+1), independently of atan2.
      const double tangent = tiny ? 1e-8 / (std::sqrt(1.0 + 1e-16) + 1.0) : 1.0;
      const double boundaryM = 0.5 * segmentM / tangent;
      for (const double radius : {0.0, 0.99 * boundaryM}) {
        const auto route = network.Plan({}, finish, radius);
        CHECK(route.Found && route.Legs.size() == 3 && route.TurnsRefused == 0,
              "fitting circular turn remains routable");
      }
      const auto refused = network.Plan({}, finish, 1.01 * boundaryM);
      CHECK(!refused.Found && refused.TurnsRefused == 1,
            "radius beyond available tangent length is rejected even for tiny angles");
    }
  }
  auto straight = Path::Network::Create({.CellM = 1}, {});
  CHECK(straight.has_value(), "straight grid");
  if (!straight) { return Report(); }
  const std::array<double, 6> line{0, 0, 0, longitudeStep, 0, 2.0 * longitudeStep};
  CHECK(straight->Lay(line, {.Oneway = true}).has_value(), "straight way accepted");
  std::string error;
  CHECK(straight->Weave(error), "straight way builds");
  const auto route =
      straight->Plan({}, {.LongitudeDeg = 2.0 * longitudeStep}, std::numeric_limits<double>::max());
  CHECK(route.Found && route.Legs.size() == 3 && route.TurnsRefused == 0,
        "exactly straight continuation supports every finite minimum radius");
  return Report();
}
