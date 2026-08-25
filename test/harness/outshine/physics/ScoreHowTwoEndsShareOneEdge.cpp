#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "Check.h"
#include "Wayfinding.h"

namespace {

// THE ORACLE IS THAT A ROAD DOES NOT DOUBLE BACK ON ITSELF.
//
// Two side roads meet a straight main road at two places 745 m apart. Whatever a graph does
// internally, the distance between those two junctions ALONG the main road is 745 m, because
// both lie on it and it is straight between them. A network that answers 1489 m is saying the
// only way from one to the other is out to the end of the road and back, and no such road
// exists.
//
// WHY THIS IS THE CASE. `Network::Weave` ties a loose end onto the edge it ends on by SPLITTING
// that edge: it unlinks (a,b) and links (a,loose) and (loose,b). It finds the edge through an
// index built once, before any splitting:
//
//   src/base/spatial/Wayfinding.cpp:243   byEdgeCell is built here
//   src/base/spatial/Wayfinding.cpp:321   unlink(bestFrom, bestTo)
//   src/base/spatial/Wayfinding.cpp:329   link(bestFrom, loose)
//
// so a SECOND end that ties onto the same segment is handed a pair that no longer exists. The
// unlink walks the list, finds nothing, and returns in silence; the two links are added anyway.
// What stands afterwards is not a—l1—l2—b but two parallel chords, a—l1—b beside a—l2—b, and
// the only way between l1 and l2 is through a or through b.
//
// THE GEOMETRY, worked out rather than trusted. At 48 degrees north one degree of longitude is
// 111 320 * cos(48) = 74 500 m, so:
//
//   main road      lon 11.000 to 11.020            1490 m
//   first stub meets it at                         lon 11.005
//   second stub meets it at                        lon 11.015
//   between the two junctions, along the road       745 m
//   from one junction out to the west end and back to the other
//                                    372 + 1117 =  1489 m
//
// The two answers differ by a factor of two, which is why this input separates them and why the
// tolerance below can be tight. Each stub adds its own 110 m at both ends of any route, so the
// case measures the difference between two routes rather than either alone -- the stubs cancel.
constexpr double kSphereM = 6371008.8;
constexpr double kSnapM = 8.0;

[[nodiscard]] double Along(double fromLonDeg, double toLonDeg) {
  return outshine::Path::ApartM(48.0, fromLonDeg, 48.0, toLonDeg, kSphereM);
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  outshine::Path::Network net(kSnapM, kSphereM);

  const double main_[4] = {48.0, 11.000, 48.0, 11.020};
  const double west[4] = {48.001, 11.005, 48.00001, 11.005};
  const double east[4] = {48.001, 11.015, 48.00001, 11.015};
  net.Lay(std::span<const double>(main_, 4), 4.0, 0.06, 2);
  net.Lay(std::span<const double>(west, 4), 3.0, 0.06, 1);
  net.Lay(std::span<const double>(east, 4), 3.0, 0.06, 1);

  std::string why;
  if (!net.Weave(why)) {
    Unprepared(("the network did not weave: " + why).c_str());
    return Report();
  }

  const outshine::Path::Route across =
      net.Plan(outshine::Path::Waypoint{48.001, 11.005}, outshine::Path::Waypoint{48.001, 11.015},
               0.0);
  if (!across.Found) {
    std::printf("  the two stubs are not joined at all: %s\n", across.Error.c_str());
  }

  const double straightM = Along(11.005, 11.015);
  const double aroundM = Along(11.000, 11.005) + Along(11.000, 11.015);
  const double stubsM = 2.0 * outshine::Path::ApartM(48.001, 11.005, 48.00001, 11.005, kSphereM);

  std::printf("  ends tied onto an edge          %zu\n", net.TiedToEdges());
  std::printf("  nodes %zu   edges %zu   junctions %zu\n", net.NodeCount(), net.EdgeCount(),
              net.JunctionCount());
  std::printf("  along the road between them     %8.1f m\n", straightM);
  std::printf("  out to the west end and back    %8.1f m\n", aroundM);
  std::printf("  the two stubs together          %8.1f m\n", stubsM);
  std::printf("  the route the network plans     %8.1f m\n", across.LengthM);

  CHECK(net.TiedToEdges() == 2,
        "both loose ends are tied onto the edge they end on -- without two ties there is no "
        "second tie for the first to have invalidated, and this case would measure nothing");

  CHECK(across.Found,
        "and the two side roads are joined through the main road they both meet");

  CHECK(across.LengthM < 0.5 * (aroundM + stubsM) + 0.5 * (straightM + stubsM),
        "**TWO ENDS TIED ONTO ONE SEGMENT ARE JOINED ALONG IT**: both junctions lie on the same "
        "straight road, so the distance between them is the distance along that road. A network "
        "that answers with the way out to the end and back is describing a road that doubles "
        "back on itself, which is what tying the second end against an index built before the "
        "first split produces: two parallel chords where one road should be");

  CHECK(std::fabs(across.LengthM - (straightM + stubsM)) < 0.05 * (straightM + stubsM),
        "and it is the RIGHT length: the two stubs plus the 745 m between the junctions, within "
        "five percent, which is the road as it is drawn and not a graph's opinion of it");

  Covers("wayfinding: two loose ends that meet the same segment split it once each, so the road "
         "between their junctions is the road, and a route along it does not run out to the end "
         "of the segment and back");
  return Report();
}
