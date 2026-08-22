#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "Wayfinding.h"

using outshine::Path::Network;
using outshine::Path::Route;
using outshine::Path::Waypoint;

namespace {

constexpr double kSnapM = 2.0;
constexpr double kIuggMeanRadiusM = 6371008.8;
constexpr double kDeg = 1.0e-5;

struct Laid {
  std::vector<double> Points;
  double HalfWidthM;
};

// a small mesh whose endpoints only JOIN by snapping, so the merge order is exercised:
// every way ends within kSnapM of a neighbour's start, offset by fractions of the snap
std::vector<Laid> Mesh(void) {
  std::vector<Laid> ways;
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      const double lat = 48.0 + (double)row * 800.0 * kDeg;
      const double lon = 11.0 + (double)column * 800.0 * kDeg;
      const double nudge = ((double)((row * 4 + column) % 3) - 1.0) * 1.6e-5;
      ways.push_back({{lat, lon, lat + 800.0 * kDeg + nudge, lon}, 3.5});
      ways.push_back({{lat, lon, lat, lon + 800.0 * kDeg - nudge}, 3.5});
    }
  }
  return ways;
}

Route Planned(const std::vector<Laid> &ways, bool reversed, size_t &nodes) {
  Network net(kSnapM, kIuggMeanRadiusM);
  if (reversed) {
    for (size_t at = ways.size(); at > 0; --at) {
      const Laid &way = ways[at - 1];
      net.Lay(way.Points.data(), way.Points.size() / 2, way.HalfWidthM, 0.1, 2);
    }
  } else {
    for (const Laid &way : ways) {
      net.Lay(way.Points.data(), way.Points.size() / 2, way.HalfWidthM, 0.1, 2);
    }
  }
  std::string error;
  if (!net.Weave(error)) {
    std::printf("REFUSED %s\n", error.c_str());
    return Route{};
  }
  nodes = net.NodeCount();
  return net.Plan(Waypoint{48.0, 11.0}, Waypoint{48.0 + 2400.0 * kDeg, 11.0 + 2400.0 * kDeg},
                  0.0, 10.0);
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::vector<Laid> ways = Mesh();
  size_t forwardNodes = 0, backwardNodes = 0;
  const Route forward = Planned(ways, false, forwardNodes);
  const Route backward = Planned(ways, true, backwardNodes);

  CHECK(forward.Found && backward.Found, "both arrival orders plan the crossing");
  Note("nodes woven, laid forward", (double)forwardNodes, "nodes");
  Note("nodes woven, laid backward", (double)backwardNodes, "nodes");
  Note("route laid forward", forward.LengthM, "m");
  Note("route laid backward", backward.LengthM, "m");

  CHECK(forwardNodes == backwardNodes,
        "the weave merges the same nodes whichever way the tiles arrived");
  CHECK(forward.Legs.size() == backward.Legs.size(),
        "and the plan walks the same chain of legs");
  CHECK(forward.LengthM == backward.LengthM,
        "**A ROUTE OVER THE SAME WAYS IS THE SAME ROUTE, TO THE BIT.** The weave orders the "
        "ways by their content before any node merges, so the scheduler's arrival order -- "
        "the 54 mm over 762 km -- cannot reach the graph");

  Covers("I.4.6 planning is deterministic over content: the woven network and the planned "
         "route are functions of the declared ways, never of tile arrival order");
  return Report();
}
