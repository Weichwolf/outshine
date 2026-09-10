#include "Wayfinding.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (const double radius : {kEarthMeanRadiusM, 1e32}) {
    auto created = Path::Network::Create({.CellM = radius * 1e-4}, {.RadiusM = radius});
    CHECK(created.has_value(), "scaled sphere and grid accepted");
    if (!created) { return Report(); }
    const std::array<double, 6> points{0, 0, 10, 0, 20, 0};
    CHECK(created->Lay(points, {.Oneway = true}).has_value(), "scaled meridian inserted");
    std::string error;
    CHECK(created->Weave(error), "scaled network builds");
    const auto route = created->Plan({}, {.LatitudeDeg = 20}, 0);
    const double expected = radius * (std::numbers::pi / 9.0);
    CHECK(route.Found && route.Legs.size() == 3 && std::abs(route.LengthM / expected - 1.0) < 1e-12,
          "finite route length has no arbitrary 1e30 ceiling");
  }
  const double radius = std::numeric_limits<double>::max() / 8.0;
  auto created = Path::Network::Create({.CellM = radius * 0.01}, {.RadiusM = radius});
  CHECK(created.has_value(), "finite large sphere accepted");
  if (!created) { return Report(); }
  std::vector<double> zigzag;
  for (int i = 0; i < 12; ++i) {
    zigzag.push_back(-50.0 + 9.0 * i);
    zigzag.push_back(i % 2 == 0 ? -80.0 : 80.0);
  }
  CHECK(created->Lay(zigzag, {.Oneway = true}).has_value(), "long directed chain inserted");
  std::string error;
  CHECK(created->Weave(error) && created->NodeCount() == 12 && created->EdgeCount() == 11,
        "overflow fixture remains a single directed chain");
  const auto route = created->Plan(
      {.LongitudeDeg = -80, .LatitudeDeg = -50}, {.LongitudeDeg = 80, .LatitudeDeg = 49}, 0);
  CHECK(!route.Found && route.Legs.empty() &&
            route.Error.find("finite metre range") != std::string::npos,
        "unrepresentable search costs are distinct from a disconnected graph");
  return Report();
}
