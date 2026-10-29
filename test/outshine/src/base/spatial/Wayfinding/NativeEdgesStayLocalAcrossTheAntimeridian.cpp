#include "Wayfinding.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <numbers>
#include <string>
#include <string_view>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;

LongitudeLatitude Point(double longitude, double eastM, double northM) {
  const double degreesPerM = 180.0 / (std::numbers::pi * kWgs84A);
  return {.LongitudeDeg = std::remainder(longitude + eastM * degreesPerM, 360.0),
          .LatitudeDeg = northM * degreesPerM};
}

std::expected<Path::Network, std::string_view> Fixture(double longitude, bool reverse) {
  auto network = Path::Network::Create({.CellM = 2}, {.RadiusM = kWgs84A});
  if (!network) { return network; }
  const auto west = Point(longitude, -500, 0);
  const auto east = Point(longitude, 500, 0);
  const auto near = Point(longitude, 0, 3);
  const auto far = Point(longitude, 0, 500);
  const std::array<double, 4> road =
      reverse
          ? std::array{east.LatitudeDeg, east.LongitudeDeg, west.LatitudeDeg, west.LongitudeDeg}
          : std::array{west.LatitudeDeg, west.LongitudeDeg, east.LatitudeDeg, east.LongitudeDeg};
  const std::array<double, 4> spur{
      near.LatitudeDeg, near.LongitudeDeg, far.LatitudeDeg, far.LongitudeDeg};
  for (const auto &points : {road, spur}) {
    const auto laid = network->Lay(points, {.HalfWidthM = 2, .Oneway = true});
    if (!laid) { return std::unexpected(laid.error()); }
  }
  return network;
}

void Inspect(const Path::Network &network, double longitude, bool reverse) {
  CHECK(network.NodeCount() == 4 && network.EdgeCount() == 3 && network.TiedToEdges() == 1,
        "the local road and directed spur retain one physical splice across a longitude seam");
  CHECK(network.CellsInTheTieIndex() < 1024,
        "a kilometre road occupies local index cells rather than a wrapped global rectangle");
  const auto from = Point(longitude, reverse ? 500 : -500, 0);
  const auto to = Point(longitude, reverse ? -500 : 500, 0);
  const auto junction = Point(longitude, 0, 3);
  const auto far = Point(longitude, 0, 500);
  const auto forward = network.Plan(from, to, 0);
  const auto back = network.Plan(to, from, 0);
  const double expected = Path::ApartM(from, junction, {.RadiusM = kWgs84A}) +
                          Path::ApartM(junction, to, {.RadiusM = kWgs84A});
  CHECK(forward.Found && std::abs(forward.LengthM - expected) < 1e-5 && !back.Found,
        "short directed routes preserve the actual spliced geometry on both sides of the seam");
  CHECK(network.Plan(from, far, 0).Found && !network.Plan(far, to, 0).Found,
        "the periodic index preserves directional junction access");
}

void Scenario(double longitude, bool reverse) {
  auto one = Fixture(longitude, reverse);
  CHECK(one.has_value(), "the finite source coordinates and profile are valid");
  if (!one) { return; }
  std::string error;
  CHECK(one->Weave(error), "one-shot native weaving accepts the short periodic road");
  if (!error.empty()) { return; }
  Inspect(*one, longitude, reverse);
  auto input = Fixture(longitude, reverse);
  CHECK(input.has_value(), "incremental weaving starts from the same original source");
  if (!input) { return; }
  auto job = Path::NetworkWeaveJob::Begin(std::move(*input));
  CHECK(job.has_value(), "the native bounded weave owns the same source input");
  if (!job) { return; }
  bool done = false;
  size_t calls = 0;
  while (!done && calls < 2000) {
    const auto advanced = job->Advance(1);
    CHECK(advanced.has_value(), "each bounded native index step preserves valid source geometry");
    if (!advanced) { break; }
    done = *advanced;
    ++calls;
  }
  CHECK(done && calls < 2000,
        "periodic indexing completes in bounded local work without a whole-world scan");
  if (!done) { return; }
  auto sliced = std::move(*job).Take();
  CHECK(sliced.has_value(), "the complete incremental network transfers its owner");
  if (!sliced) { return; }
  Inspect(*sliced, longitude, reverse);
  CHECK(sliced->CellsInTheTieIndex() == one->CellsInTheTieIndex(),
        "both execution paths index exactly the same local cells");
}
}

int main() {
  for (const double longitude : {0.0, 179.999999, -179.999999, 180.0, -180.0}) {
    for (const bool reverse : {false, true}) { Scenario(longitude, reverse); }
  }
  return Report();
}
