#include "Wayfinding.h"
#include "Check.h"
#include <array>
#include <string>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Path::Network empty({.CellM = 1}, {});
  const auto none = empty.WeakComponents();
  CHECK(none.Count == 0 && none.Largest == 0 && none.UnderFour == 0 && none.InUnderFour == 0,
        "empty graph has no components");
  for (const bool reverse : {false, true}) {
    Path::Network network({.CellM = 1}, {});
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
      network.Lay(way, {.Oneway = true});
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
