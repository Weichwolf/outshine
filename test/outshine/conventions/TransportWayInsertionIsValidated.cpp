#include "Wayfinding.h"
#include "Check.h"
#include <array>
#include <limits>
#include <span>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto networkResult = Path::Network::Create({.CellM = 1}, {});
  CHECK(networkResult.has_value(), "valid network configuration accepted");
  if (!networkResult) { return Report(); }
  auto &network = *networkResult;
  const std::array<double, 4> line{0, 0, 0, 0.01};
  CHECK(network.Lay(line, {}).has_value(), "valid initial way accepted");
  std::vector<size_t> found{123};
  const auto needsBuild = [&] {
    const auto nearest = network.Nearest({});
    CHECK(!nearest && nearest.error().find("rebuilt") != std::string_view::npos,
          "unbuilt sources cannot appear as an empty or stale nearest result");
    CHECK(!network.Within({}, 1000, found) && found == std::vector<size_t>{123},
          "unbuilt source query preserves the destination");
  };
  needsBuild();
  std::string error;
  CHECK(network.Weave(error), "initial network builds");
  const auto capacity = network.PointStreamHeldBytes();
  const auto preserved = [&] {
    CHECK(network.WayCount() == 1 && network.PointCount() == 2 && network.NodeCount() == 2 &&
              network.EdgeCount() == 2 && network.PointStreamHeldBytes() == capacity,
          "rejection preserves source arrays, graph and allocated point capacity");
    CHECK(network
              .Plan({.LongitudeDeg = 0, .LatitudeDeg = 0},
                    {.LongitudeDeg = 0.01, .LatitudeDeg = 0},
                    0)
              .Found,
          "rejection preserves routing readiness");
    CHECK(network.Nearest({}).has_value(), "rejected insertion preserves spatial readiness");
  };
  for (const auto points : {std::span<const double>{},
                            std::span<const double>(line).first(2),
                            std::span<const double>(line).first(3)}) {
    CHECK(!network.Lay(points, {}), "empty, short and incomplete coordinates rejected");
    preserved();
  }
  for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity(),
                               91.0,
                               -91.0}) {
    const std::array<double, 6> points{0, 0, 0, 0.01, invalid, 0};
    CHECK(!network.Lay(points, {}), "invalid final coordinate rejects the entire way");
    preserved();
  }
  for (const auto field : {&Path::WayClass::HalfWidthM,
                           &Path::WayClass::MaxGradient,
                           &Path::WayClass::MinRadiusM,
                           &Path::WayClass::Friction,
                           &Path::WayClass::SpeedMps}) {
    for (const double value : {-1.0,
                               std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity()}) {
      Path::WayClass properties;
      properties.*field = value;
      CHECK(!network.Lay(line, properties), "invalid physical property rejected");
      preserved();
    }
  }
  CHECK(!network.Lay(line, {.Lanes = -1}), "negative lane count rejected");
  CHECK(!network.Lay(line, {.HalfWidthM = std::numeric_limits<double>::max()}),
        "full width must remain representable");
  preserved();
  const std::vector<double> excessive(2 * Path::kMaxNetworkPoints);
  CHECK(!network.Lay(excessive, {}), "cumulative point budget enforced before append");
  preserved();
  CHECK(network.Lay(line, {.Oneway = true}).has_value() && network.WayCount() == 2,
        "valid insertion recovers after all failures");
  CHECK(
      !network
           .Plan({.LongitudeDeg = 0, .LatitudeDeg = 0}, {.LongitudeDeg = 0.01, .LatitudeDeg = 0}, 0)
           .Found,
      "successful insertion requires graph rebuilding");
  needsBuild();
  CHECK(network.Weave(error), "updated source can rebuild");
  const auto nearest = network.Nearest({});
  CHECK(nearest && *nearest, "rebuilt graph resumes nearest queries");
  CHECK(network.Within({}, 0, found).has_value() && found.size() == 1,
        "rebuilt graph replaces query output with current results");
  return Report();
}
