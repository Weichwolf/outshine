#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "Wayfinding.h"

using outshine::World::ApartM;
using outshine::World::Network;
using outshine::World::Route;
using outshine::World::Waypoint;

namespace {

constexpr double kSnapM = 2.0;
constexpr double kBlockDeg = 0.01;
constexpr int kSide = 6;

double MetresPerDegreeLat(void) { return ApartM(0.0, 0.0, 1.0, 0.0); }

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Note("a degree of latitude", MetresPerDegreeLat(), "m");
  CHECK_NEAR(MetresPerDegreeLat(), 111194.9, 1.0, "m",
             "**DISTANCE IS ON A SPHERE AND NOT ON A PLANE.** A degree of latitude is 111.195 km on "
             "a sphere of the WGS84 mean radius, and a route 775 km long that used a flat "
             "approximation would be wrong by kilometres");
  Note("Munich to Hamburg as the crow flies",
       ApartM(48.1371, 11.5754, 53.5503, 9.9920) / 1000.0, "km");
  CHECK_NEAR(ApartM(48.1371, 11.5754, 53.5503, 9.9920) / 1000.0, 612.0, 5.0, "km",
             "and Marienplatz to Rathausmarkt is 612 km, which is the number every route length in "
             "this case is read against");

  Note("a metre either side of the antimeridian", ApartM(0.0, 179.99999, 0.0, -179.99999), "m");
  CHECK(ApartM(0.0, 179.99999, 0.0, -179.99999) < 5.0,
        "**AND TWO POINTS EITHER SIDE OF THE 180TH MERIDIAN ARE NEIGHBOURS.** Subtracting the "
        "longitudes gives 360 degrees and 40 000 km; wrapping the difference gives 2 m, which is "
        "what they are. board:1524's antimeridian stratum exists because this is silent when wrong");

  Network grid(kSnapM);
  double lat = 48.0, lon = 11.0;
  for (int row = 0; row < kSide; ++row) {
    std::vector<double> along;
    for (int column = 0; column < kSide; ++column) {
      along.push_back(lat + (double)row * kBlockDeg);
      along.push_back(lon + (double)column * kBlockDeg);
    }
    grid.Lay(along.data(), (size_t)kSide, 3.5, 0.06, 2);
  }
  for (int column = 0; column < kSide; ++column) {
    std::vector<double> down;
    for (int row = 0; row < kSide; ++row) {
      down.push_back(lat + (double)row * kBlockDeg);
      down.push_back(lon + (double)column * kBlockDeg);
    }
    grid.Lay(down.data(), (size_t)kSide, 3.5, 0.06, 2);
  }

  std::string error;
  const bool woven = grid.Weave(error);
  if (!woven) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(woven, "a network is woven from the ways laid into it");
  if (!woven) { return Report(); }

  Note("ways laid", (double)grid.WayCount(), "ways");
  Note("points in them", (double)grid.PointCount(), "points");
  Note("nodes after snapping", (double)grid.NodeCount(), "nodes");
  Note("junctions among them", (double)grid.JunctionCount(), "nodes");
  Note("the snapping distance", grid.SnapM(), "m");

  CHECK(grid.NodeCount() == (size_t)(kSide * kSide),
        "**AND THE WAYS MEET WHERE THEY CROSS, WHICH NOBODY TOLD THEM.** 12 ways of 6 points is 72 "
        "points and 36 nodes: a vector tile carries no OSM node identity and clips every way at "
        "the tile border, so two roads that meet are two coordinates that happen to agree. "
        "Snapping is the only thing that can join them");
  CHECK(grid.JunctionCount() == (size_t)(kSide * kSide - 4),
        "and every node except the FOUR CORNERS is a junction: 16 in the interior at degree four "
        "and 16 on the edges at degree three, because an edge node has two ways along the edge and "
        "one leading inward. Only a corner has just two. That the count lands on 32 and not on 72 "
        "or on 36 is what says the snap joined the right pairs and not merely a lot of them");

  size_t node = 0;
  double awayM = 0.0;
  CHECK(grid.Nearest(Waypoint{lat + 0.5 * kBlockDeg, lon + 0.5 * kBlockDeg}, node, awayM),
        "a coordinate off the network resolves to the node nearest it");
  Note("how far the nearest node was", awayM, "m");
  CHECK(awayM > 0.0 && awayM < 1000.0, "and it is a node of this grid rather than the first one");

  const Waypoint corner{lat, lon};
  const Waypoint opposite{lat + (double)(kSide - 1) * kBlockDeg,
                          lon + (double)(kSide - 1) * kBlockDeg};
  const Route across = grid.Plan(corner, opposite, 0.0, 10.0);
  if (!across.Found) { std::printf("REFUSED %s\n", across.Error.c_str()); }
  CHECK(across.Found, "and a route is searched from one corner of it to the other");
  if (!across.Found) { return Report(); }

  const double sideM = ApartM(lat, lon, lat + (double)(kSide - 1) * kBlockDeg, lon);
  const double acrossM = ApartM(lat + (double)(kSide - 1) * kBlockDeg, lon,
                                lat + (double)(kSide - 1) * kBlockDeg,
                                lon + (double)(kSide - 1) * kBlockDeg);
  Note("the route it found", across.LengthM, "m");
  Note("what a grid with no diagonal must cost", sideM + acrossM, "m");
  Note("the straight line between the corners", across.StraightM, "m");
  Note("legs in the route", (double)across.Legs.size(), "legs");
  Note("nodes the search settled", (double)across.Reached, "of 36");

  CHECK_NEAR(across.LengthM, sideM + acrossM, 1.0, "m",
             "**AND IT IS THE SHORTEST ROUTE AND NOT MERELY A ROUTE.** On a grid with no diagonals "
             "every path from corner to corner costs one side plus one crossing, and the search "
             "returns exactly that -- so A* with a great-circle heuristic is admissible here "
             "rather than merely fast");
  CHECK(across.LengthM > across.StraightM,
        "and it is longer than the straight line, because roads do not go where crows do");
  CHECK(across.Legs.size() == (size_t)(2 * kSide - 1),
        "with one leg per node it passed through");
  CHECK(across.Reached <= grid.NodeCount(),
        "having settled no more nodes than the network holds, which is what makes the heuristic "
        "worth carrying");

  Network island(kSnapM);
  const double first[4] = {48.0, 11.0, 48.0, 11.01};
  const double second[4] = {49.0, 12.0, 49.0, 12.01};
  island.Lay(first, 2, 3.5, 0.06, 2);
  island.Lay(second, 2, 3.5, 0.06, 2);
  CHECK(island.Weave(error), "two ways that never meet still weave");
  const Route broken = island.Plan(Waypoint{48.0, 11.0}, Waypoint{49.0, 12.0}, 0.0, 10.0);
  CHECK(!broken.Found,
        "**AND A NETWORK IN PIECES IS A NAMED REFUSAL AND NOT AN EMPTY ROUTE.** The search says how "
        "many nodes it could reach, so a caller can tell a disconnected network from a search that "
        "gave up -- which is the difference between a data finding and an engine defect");
  std::printf("REFUSAL %s\n", broken.Error.c_str());

  Covers("I.4.4 a network is woven from ways that share no identity: points within a declared "
         "snapping distance become one node, ways meet where they cross, and A* over it with a "
         "great-circle heuristic returns the shortest route or names why there is none");
  return Report();
}
