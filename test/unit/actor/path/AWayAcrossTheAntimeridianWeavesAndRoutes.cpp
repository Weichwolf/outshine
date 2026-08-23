#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "Wayfinding.h"

using outshine::Path::Network;
using outshine::Path::Route;
using outshine::Path::Waypoint;

namespace {

constexpr double kSnapM = 10.0;
constexpr double kIuggMeanRadiusM = 6371008.8;
constexpr double kLatDeg = 60.0;

void Lay(Network &net, double fromLon, double toLon) {
  const double points[4] = {kLatDeg, fromLon, kLatDeg, toLon};
  net.Lay(points, 2, 3.5, 0.1, 2);
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // two ways END at the date line, 5.6 m apart across it -- inside the snap, so they are
  // ONE junction; an index that hashes raw longitude puts them half a billion columns
  // apart and the network weaves in pieces
  Network net(kSnapM, kIuggMeanRadiusM);
  Lay(net, 179.98, 179.99995);
  Lay(net, -179.99995, -179.98);
  std::string error;
  CHECK(net.Weave(error), "the two ways weave");
  CHECK(net.NodeCount() == 3,
        "**THE ANTIMERIDIAN'S TWO SIDES ARE NEIGHBOURS**: the ends 5.6 m apart across the "
        "line snap to ONE node -- four nodes would be a network in pieces (board:1719)");

  const Route across = net.Plan(Waypoint{kLatDeg, 179.981}, Waypoint{kLatDeg, -179.981}, 5.0);
  if (!across.Found) { std::printf("REFUSED %s\n", across.Error.c_str()); }
  CHECK(across.Found, "**AND A ROUTE CROSSES THE LINE** instead of refusing at the seam");
  Note("the crossing runs", across.LengthM, "m");
  CHECK(across.LengthM > 2000.0 && across.LengthM < 2500.0,
        "over ~2.2 km of road -- never the long way round the sphere, and never a "
        "360-degree pseudo-heading judged as a turn");
  // (TurnsRefused counts the U-turn reversals the search declines along the way; the
  // discriminator is Found itself -- an unwrapped lon delta reads the straight crossing
  // as a ~360-degree hairpin and refuses the route entirely)

  Covers("I.9.13 the cell index lives on the sphere it indexes: a row's own latitude fixes "
         "the column modulus, the column wraps at the row's circumference, and a way "
         "meeting its neighbour across the antimeridian weaves and routes (board:1719, "
         "the route class 1524 names)");
  return Report();
}
