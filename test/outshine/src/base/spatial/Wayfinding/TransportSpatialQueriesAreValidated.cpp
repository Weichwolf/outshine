#include "Wayfinding.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <string>
#include <vector>

namespace {
double Distance(outshine::LongitudeLatitude a, outshine::LongitudeLatitude b) {
  constexpr double radians = std::numbers::pi / 180.0;
  const auto xyz = [&](outshine::LongitudeLatitude p) {
    const double lat = p.LatitudeDeg * radians;
    const double lon = p.LongitudeDeg * radians;
    return std::array{std::cos(lat) * std::cos(lon), std::cos(lat) * std::sin(lon), std::sin(lat)};
  };
  const auto x = xyz(a);
  const auto y = xyz(b);
  const double cross =
      std::hypot(x[1] * y[2] - x[2] * y[1], x[2] * y[0] - x[0] * y[2], x[0] * y[1] - x[1] * y[0]);
  const double dot = x[0] * y[0] + x[1] * y[1] + x[2] * y[2];
  return outshine::kEarthMeanRadiusM * std::atan2(cross, dot);
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto created = Path::Network::Create({.CellM = 1}, {});
  CHECK(created.has_value(), "valid grid");
  if (!created) { return Report(); }
  auto &network = *created;
  std::vector<size_t> found{123};
  const auto empty = network.Nearest({});
  CHECK(empty && !*empty, "empty network is distinct from invalid query");
  for (const double invalid : {-1.0,
                               std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity()}) {
    CHECK(!network.Within({}, invalid, found) && found == std::vector<size_t>{123},
          "invalid radius preserves output");
  }
  for (const double invalid : {181.0,
                               -181.0,
                               std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity()}) {
    for (const auto at :
         {LongitudeLatitude{.LongitudeDeg = invalid}, LongitudeLatitude{.LatitudeDeg = invalid}}) {
      CHECK(!network.Nearest(at), "nearest rejects invalid coordinates even on empty graph");
      CHECK(!network.Within(at, 0, found) && found == std::vector<size_t>{123},
            "invalid coordinate preserves output");
    }
  }
  std::vector<LongitudeLatitude> points;
  for (const double lat : {-89.0, 0.0, 89.0}) {
    for (const double lon : {-179.999, -120.0, -60.0, 0.0, 60.0, 120.0, 179.999}) {
      points.push_back({.LongitudeDeg = lon, .LatitudeDeg = lat});
      const std::array way{lat, lon, lat, lon};
      CHECK(network.Lay(way, {}).has_value(), "isolated point inserted");
    }
  }
  std::string error;
  CHECK(network.Weave(error) && network.NodeCount() == points.size(), "distinct point grid builds");
  for (const auto at : {LongitudeLatitude{},
                        LongitudeLatitude{.LongitudeDeg = 180},
                        LongitudeLatitude{.LatitudeDeg = 90},
                        LongitudeLatitude{.LongitudeDeg = 60, .LatitudeDeg = 89}}) {
    for (const double radius : {0.0, 0.1, 2.0, 1000.0, std::numeric_limits<double>::max()}) {
      const auto expected =
          std::ranges::count_if(points, [&](auto point) { return Distance(at, point) <= radius; });
      CHECK(network.Within(at, radius, found).has_value(),
            "valid search succeeds including maximum finite radius");
      CHECK(found.size() == static_cast<size_t>(expected),
            "indexed and full scans match independent spherical oracle");
    }
    const auto nearest = network.Nearest(at);
    double expected = std::numeric_limits<double>::max();
    for (const auto point : points) { expected = std::min(expected, Distance(at, point)); }
    CHECK(nearest && *nearest && std::abs((*nearest)->AwayM - expected) < 1e-5,
          "nearest distance matches independent global minimum");
  }
  return Report();
}
