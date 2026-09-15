#include "Wayfinding.h"
#include "Check.h"
#include <array>
#include <optional>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto created = Path::Network::Create({.CellM = 1}, {});
  CHECK(created.has_value(), "valid network");
  if (!created) { return Report(); }
  auto &network = *created;
  const std::array<double, 4> north{0, 0, 0.01, 0};
  const std::array<double, 4> east{0, 0, 0, 0.01};
  CHECK(network.Lay(north, {}).has_value(), "north branch accepted");
  CHECK(network.Lay(east, {}).has_value(), "east branch accepted");
  std::string error;
  CHECK(network.Weave(error), "shared junction built");
  unsigned sharedCalls = 0;
  unsigned calls = 0;
  const auto first = network.Elevate([&](LongitudeLatitude at) -> std::optional<double> {
    ++calls;
    if (at.LatitudeDeg == 0 && at.LongitudeDeg == 0 && ++sharedCalls == 1) { return std::nullopt; }
    return 40.0;
  });
  CHECK(calls == 3 && sharedCalls == 1, "three distinct nodes sampled once each");
  CHECK(first.Points == 2 && first.Refused == 2, "both copies retain missing junction sample");
  for (size_t way = 0; way < 2; ++way) {
    const auto start = network.Profile({.Way = way, .StationM = 0});
    CHECK(start && start->HeightM == 0, "both branch starts use missing-height fallback");
  }
  calls = 0;
  const auto next = network.Elevate([&](LongitudeLatitude) -> std::optional<double> {
    ++calls;
    return 40.0;
  });
  CHECK(calls == 3 && next.Points == 4 && next.Refused == 0, "next update resamples all nodes");
  for (size_t way = 0; way < 2; ++way) {
    const auto start = network.Profile({.Way = way, .StationM = 0});
    CHECK(start && start->HeightM == 40 && start->Grade == 0, "new height reaches both branches");
  }
  return Report();
}
