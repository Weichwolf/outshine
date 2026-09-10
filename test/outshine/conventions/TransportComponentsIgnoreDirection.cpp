#include "Wayfinding.h"
#include "Check.h"
#include <array>
#include <string>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto emptyResult = Path::Network::Create({.CellM = 1}, {});
  CHECK(emptyResult.has_value(), "valid network configuration accepted");
  if (!emptyResult) { return Report(); }
  auto &empty = *emptyResult;
  const auto none = empty.WeakComponents();
  CHECK(none.Count == 0 && none.Largest == 0 && none.UnderFour == 0 && none.InUnderFour == 0,
        "empty graph has no components");
  for (const bool reverse : {false, true}) {
    auto networkResult = Path::Network::Create({.CellM = 1}, {});
    CHECK(networkResult.has_value(), "valid network configuration accepted");
    if (!networkResult) { return Report(); }
    auto &network = *networkResult;
    std::array ways{std::array<double, 4>{-0.01, -0.01, 0, 0},
                    std::array<double, 4>{-0.01, 0.01, 0, 0},
                    std::array<double, 4>{0.01, 0, 0, 0},
                    std::array<double, 4>{10, 10, 10, 10.01},
                    std::array<double, 4>{20, 20, 20, 20}};
    for (auto way : ways) {
      if (reverse) {
        std::swap(way[0], way[2]);
        std::swap(way[1], way[3]);
      }
      CHECK(network.Lay(way, {.Oneway = true}).has_value(), "valid transport way accepted");
    }
    std::string error;
    CHECK(network.Weave(error), "directed fixture with three physical components builds");
    const auto pieces = network.WeakComponents();
    CHECK(network.NodeCount() == 7 && pieces.Count == 3 && pieces.Largest == 4 &&
              pieces.UnderFour == 2 && pieces.InUnderFour == 3,
          "converging and diverging directions preserve component sizes four, two and one");
    const auto start = network.Nearest({.LongitudeDeg = -0.01, .LatitudeDeg = -0.01});
    CHECK(start.has_value(), "leaf node exists");
    if (start) {
      const std::array seed{start->Node};
      CHECK(network.Reaches(seed) == (reverse ? 1u : 2u),
            "directed reachability remains distinct from physical connectivity");
    }
  }
  return Report();
}
