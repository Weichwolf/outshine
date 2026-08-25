#include <cmath>
#include <numbers>
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
constexpr double kLatDeg = 48.0;
// derived: metres per degree on the IUGG sphere, and the parallel's shortening at 48N
const double kMPerLatDeg = kIuggMeanRadiusM * std::numbers::pi / 180.0;
const double kMPerLonDeg = kMPerLatDeg * std::cos(kLatDeg * std::numbers::pi / 180.0);

struct At {
  double EastM;
  double NorthM;
  [[nodiscard]] double Lat(void) const { return kLatDeg + NorthM / kMPerLatDeg; }
  [[nodiscard]] double Lon(void) const { return 11.0 + EastM / kMPerLonDeg; }
};

void Lay(Network &net, At from, At to) {
  const double points[4] = {from.Lat(), from.Lon(), to.Lat(), to.Lon()};
  net.Lay(std::span<const double>(points, 2 * 2), 3.5, 0.1, 2);
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // the junction M is reachable two ways: the short approach S->M arrives heading east and
  // the onward hairpin M->T refuses for a car that needs 5 m of turning room; the long way
  // round S->U->V->E->M arrives heading WEST, and the same onward edge is a 45-degree
  // drift. A search that settles NODES seals M via the cheap approach and reads the world
  // as a network in pieces; the turn belongs to the edge pair, so the state is the edge.
  const At s{0.0, 0.0}, m{1000.0, 0.0}, t{990.0, 10.0};
  const At u{0.0, 600.0}, v{2000.0, 600.0}, e{2000.0, 0.0};
  Network net(kSnapM, kIuggMeanRadiusM);
  Lay(net, s, m);
  Lay(net, m, t);
  Lay(net, s, u);
  Lay(net, u, v);
  Lay(net, v, e);
  Lay(net, e, m);
  std::string error;
  CHECK(net.Weave(error), "the six ways weave");

  const Route legal =
      net.Plan(Waypoint{s.Lat(), s.Lon()}, Waypoint{t.Lat(), t.Lon()}, 5.0);
  if (!legal.Found) { std::printf("REFUSED %s\n", legal.Error.c_str()); }
  CHECK(legal.Found,
        "**A TURN REFUSAL BELONGS TO THE APPROACH, NOT THE JUNCTION**: the route exists the "
        "long way round, and a node-settled search called this a network in pieces "
        "(board:1710)");
  Note("the legal route runs", legal.LengthM, "m");
  CHECK(legal.LengthM > 4000.0,
        "and it IS the long way -- 4200 m of detour, never the 1014 m hairpin the car "
        "cannot take");
  CHECK(legal.TurnsRefused > 0,
        "the hairpin was seen and refused on the short approach -- the refusal is real, "
        "only its scope moved");

  const Route nimble =
      net.Plan(Waypoint{s.Lat(), s.Lon()}, Waypoint{t.Lat(), t.Lon()}, 0.0);
  CHECK(nimble.Found && nimble.LengthM < 2100.0,
        "a vehicle without a turning bound still takes the hairpin -- the detour was the "
        "GEOMETRY'S price, not the planner's habit");

  {
    // the woven cell index answers the spatial questions -- exactly, at the boundary
    std::vector<size_t> held;
    net.Within(Waypoint{s.Lat(), s.Lon()}, 650.0, held);
    CHECK(held.size() == 2,
          "**WITHIN READS THE WOVEN INDEX AND MISSES NOTHING AT THE RIM**: 650 m around the "
          "start holds exactly S and U (600 m) -- M at 1000 m stays out (board:1711)");
    net.Within(Waypoint{s.Lat(), s.Lon()}, 20000.0, held);
    CHECK(held.size() == 6, "and a reach past the whole net answers every node once");
    size_t node = 0;
    double awayM = 0.0;
    CHECK(net.Nearest(Waypoint{s.Lat() + 3.0, s.Lon()}, node, awayM) && awayM > 300000.0 &&
              awayM < 340000.0,
          "a waypoint three degrees away still finds its nearest node -- the widening "
          "reaches past the index into the scan instead of walking a million empty rings");
  }

  Covers("I.101 the route search walks edge states: a turn refusal is a property of the "
         "arriving and leaving edge pair, a junction sealed by one approach reopens for "
         "another, and the long legal way is found where node settling refused (board:1710)");
  return Report();
}
