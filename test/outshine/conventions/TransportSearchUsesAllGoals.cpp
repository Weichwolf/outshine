#include "Wayfinding.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <numbers>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr double radians = std::numbers::pi / 180.0;
  constexpr double radiusM = kEarthMeanRadiusM;
  const LongitudeLatitude start{.LongitudeDeg = 0, .LatitudeDeg = 0};
  const LongitudeLatitude target{.LongitudeDeg = 0.01, .LatitudeDeg = 0};
  const double directM = radiusM * 0.01 * radians;
  const double shorterM =
      radiusM * std::acos(std::cos(0.0015 * radians) * std::cos(0.009 * radians));
  CHECK(shorterM < directM, "independent spherical lengths distinguish the two direct routes");
  for (const double sign : {-1.0, 1.0}) {
    auto networkResult = Path::Network::Create({.CellM = 1}, {});
    CHECK(networkResult.has_value(), "valid network configuration accepted");
    if (!networkResult) { return Report(); }
    auto &network = *networkResult;
    const std::array<double, 6> main{0, 0, 0, 0.01, 0, 0.02};
    const std::array<double, 6> alternative{0, 0, sign * 0.0015, 0.009, sign * 0.003, 0.018};
    CHECK(network.Lay(main, {.HalfWidthM = 110, .Oneway = true}).has_value(),
          "valid transport way accepted");
    CHECK(network.Lay(alternative, {.Oneway = true}).has_value(), "valid transport way accepted");
    std::string error;
    CHECK(network.Weave(error), "two direct routes with non-goal continuations build");
    CHECK(network.NodeCount() == 5 && network.TiedToEdges() == 0,
          "fixture has exactly five nodes and no inferred connecting edges");
    const auto route = network.Plan(start, target, 0);
    CHECK(route.Found && route.StartedFrom == 1 && route.ArrivedAt == 2,
          "one start and both intended goal candidates are searched");
    CHECK(route.Found && std::abs(route.LengthM - shorterM) < 0.001,
          "shortest permitted route wins instead of the geographically nearest goal");
    CHECK(route.Legs.size() == 2 && route.Legs.back().At.LongitudeDeg == 0.009,
          "route ends at the cheaper accepted goal");
  }
  return Report();
}
