#include "Wayfinding.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <numbers>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (const double direction : {-1.0, 1.0}) {
    auto created = Path::Network::Create({.CellM = 1}, {});
    CHECK(created.has_value(), "valid grid");
    if (!created) { return Report(); }
    auto &network = *created;
    const std::array<double, 6> points{0, 0, direction * 0.01, 0, direction * 0.03, 0};
    CHECK(network
              .Lay(points,
                   {.HalfWidthM = 2,
                    .MaxGradient = 0.1,
                    .MinRadiusM = 5,
                    .Friction = 0.8,
                    .Lanes = 2,
                    .Oneway = true})
              .has_value(),
          "attributed directed chain accepted");
    std::string error;
    CHECK(network.Weave(error), "chain builds");
    const auto route = network.Plan({}, {.LatitudeDeg = direction * 0.03}, 0);
    CHECK(route.Found && route.Legs.size() == 3, "entire chain reconstructed");
    if (route.Legs.size() != 3) { continue; }
    constexpr std::array<double, 3> degrees{0, 0.01, 0.03};
    for (size_t i = 0; i < degrees.size(); ++i) {
      const auto &leg = route.Legs[i];
      const double expectedM = kEarthMeanRadiusM * degrees[i] * std::numbers::pi / 180.0;
      CHECK(leg.At.LatitudeDeg == direction * degrees[i] && leg.At.LongitudeDeg == 0,
            "positions follow start-to-goal order");
      CHECK(std::abs(leg.AlongM - expectedM) < 1e-6,
            "stations match independent meridian arc lengths");
      CHECK(leg.HalfWidthM == 2 && leg.MaxGradient == 0.1 && leg.MinRadiusM == 5 &&
                leg.Friction == 0.8 && leg.Lanes == 2,
            "route preserves physical attributes");
    }
    CHECK(route.LengthM == route.Legs.back().AlongM, "route length equals final station");
    const auto single = network.Plan({}, {}, 0);
    CHECK(single.Found && single.Legs.size() == 1 && single.LengthM == 0 &&
              single.Legs[0].AlongM == 0,
          "zero-edge route contains one zero-station endpoint");
  }
  return Report();
}
