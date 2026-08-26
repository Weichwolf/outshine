#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "Check.h"
#include "Wayfinding.h"

namespace {

// TWO ROADS THAT CROSS ON THE GROUND MEET. That is not a fact about OSM; it is a fact about
// roads. OSM records a shared node when a mapper drew one and often does not, so a graph built
// from the topology alone falls into pieces that the world does not have -- measured on the
// shipped Munich extract: 6340 places where two ways cross in plan without sharing a node, and
// 563 pieces the graph fell into.
//
// outshine is not a routing engine and this is where the difference bites. A routing engine
// trusts the topology, because inventing a junction invents a turn a vehicle cannot make and it
// would send a real driver into a wall. CLAUDE.md says the data is a source of SHAPE, never a
// specification to be reproduced, so a world where two crossing streets meet is the plausible
// one and a world in 563 pieces is not.
//
// THE EVIDENCE THAT BOUNDS IT is bridge and tunnel. Where one of the two ways spans the other,
// they do NOT meet, and joining them would fuse a motorway to the street beneath it. The vector
// schema carries no `layer` key at all -- the twelve keys are kind, rail, link, oneway_reverse,
// bridge, oneway, tunnel, surface, service, bicycle, horse, tracktype -- so those two booleans
// are the whole of the third dimension the data offers, and until they could be DECODED
// (board:1813) there was no rule to write.
//
// This case is the rule, both ways round:
//
//   two ways crossing at grade   -> one junction, and a route crosses between them
//   one of them spanning         -> no junction, and the same route refuses
//
// The second half is what keeps the first honest. A pass that joined every crossing would satisfy
// the first check and be wrong about every bridge in the world.
constexpr double kSphereM = 6371008.8;
constexpr double kSnapM = 8.0;

[[nodiscard]] outshine::Path::Route Across(bool spanning, size_t &joined, size_t &leftAlone) {
  outshine::Path::Network net(kSnapM, kSphereM);
  const double eastWest[4] = {48.0, 11.000, 48.0, 11.010};
  const double northSouth[4] = {47.998, 11.005, 48.002, 11.005};
  net.Lay(std::span<const double>(eastWest, 4), outshine::Path::WayClass{4.0, 0.06, 0.0, 1.0, 2, false});
  net.Lay(std::span<const double>(northSouth, 4), outshine::Path::WayClass{4.0, 0.06, 0.0, 1.0, 2, spanning});
  joined = net.Cross();
  leftAlone = net.CrossingsLeftAlone();
  std::string why;
  if (!net.Weave(why)) { return outshine::Path::Route{}; }
  return net.Plan(outshine::Path::Waypoint{47.998, 11.005},
                  outshine::Path::Waypoint{48.0, 11.010}, 0.0);
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  size_t joinedFlat = 0, aloneFlat = 0, joinedOver = 0, aloneOver = 0;
  const outshine::Path::Route flat = Across(false, joinedFlat, aloneFlat);
  const outshine::Path::Route over = Across(true, joinedOver, aloneOver);

  const double owedM =
      outshine::Path::ApartM(47.998, 11.005, 48.0, 11.005, kSphereM) +
      outshine::Path::ApartM(48.0, 11.005, 48.0, 11.010, kSphereM);

  std::printf("  both at grade    joined %zu  left alone %zu   route %s %8.1f m\n", joinedFlat,
              aloneFlat, flat.Found ? "found" : "REFUSED", flat.LengthM);
  std::printf("  one of them spans joined %zu  left alone %zu   route %s %8.1f m\n", joinedOver,
              aloneOver, over.Found ? "found" : "REFUSED", over.LengthM);
  std::printf("  the two legs owe                                     %8.1f m\n", owedM);

  CHECK(joinedFlat == 1 && aloneFlat == 0,
        "the sweep finds the one crossing and joins it, so there IS a junction for the route "
        "below to use -- a case where nothing crossed would prove nothing");

  CHECK(flat.Found && std::fabs(flat.LengthM - owedM) < 0.02 * owedM,
        "**TWO WAYS THAT CROSS ON THE GROUND MEET**: neither carries a bridge or a tunnel, so "
        "they are at the same height and a vehicle turns from one onto the other. The route is "
        "the two legs and nothing else. Without the junction the graph is in pieces the world "
        "does not have");

  CHECK(joinedOver == 0 && aloneOver == 1,
        "and where one way SPANS the other the crossing is left alone: the sweep counts it and "
        "makes no node, because bridge and tunnel are the whole of the third dimension this data "
        "carries");

  CHECK(!over.Found,
        "and the control is a control: the same two ways with one of them spanning are NOT "
        "joined, and the same route refuses. A pass that joined every crossing would pass the "
        "check above and be wrong about every bridge in the world");

  Covers("wayfinding: two ways that cross in plan with neither spanning the other are one "
         "junction, and a way marked as a bridge or a tunnel crosses over rather than meeting");
  return Report();
}
