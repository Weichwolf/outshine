#include "Wayfinding.h"
#include "Check.h"
#include <array>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto made = Path::Network::Create({.CellM = 1}, {});
  CHECK(made.has_value(), "valid network configuration accepted");
  if (!made) { return Report(); }
  auto &network = *made;
  const std::array<double, 4> across{0, -0.02, 0, 0.02};
  const std::array<double, 4> centre{-0.02, 0, 0.02, 0};
  CHECK(network.Lay(across, {}).has_value() && network.Lay(centre, {}).has_value(),
        "crossing fixture accepted");
  std::vector<Path::Network::Crossing> crossings;
  const auto first = network.Crossings(crossings);
  CHECK(first && first->Found == 1 && crossings.size() == 1,
        "first crossing query finds the source intersection");
  const auto cached = network.Crossings(crossings);
  CHECK(cached && cached->Found == 1 && crossings.size() == 1,
        "repeated crossing query preserves its result");

  const std::array<double, 4> offset{-0.02, 0.01, 0.02, 0.01};
  CHECK(network.Lay(offset, {}).has_value(), "source mutation accepted");
  const auto expanded = network.Crossings(crossings);
  CHECK(expanded && expanded->Found == 2 && crossings.size() == 2,
        "source mutation invalidates the crossing result");

  CHECK(network.Cross() == 2, "crossings become explicit source points");
  const auto divided = network.Crossings(crossings);
  CHECK(divided && divided->Found == 0 && crossings.empty(),
        "topology mutation invalidates crossing indices and results");
  return Report();
}
