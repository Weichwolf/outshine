#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "Wayfinding.h"

using outshine::Path::ApartM;

constexpr double kIuggMeanRadiusM = 6371008.8;
using outshine::Path::Network;
using outshine::Path::Route;
using outshine::Path::Waypoint;

namespace {

constexpr double kSnapM = 2.0;
constexpr double kBlockDeg = 0.01;
constexpr int kSide = 6;

double MetresPerDegreeLat(void) { return ApartM(0.0, 0.0, 1.0, 0.0, kIuggMeanRadiusM); }

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
       ApartM(48.1371, 11.5754, 53.5503, 9.9920, kIuggMeanRadiusM) / 1000.0, "km");
  CHECK_NEAR(ApartM(48.1371, 11.5754, 53.5503, 9.9920, kIuggMeanRadiusM) / 1000.0, 612.0, 5.0, "km",
             "and Marienplatz to Rathausmarkt is 612 km, which is the number every route length in "
             "this case is read against");

  Note("a metre either side of the antimeridian", ApartM(0.0, 179.99999, 0.0, -179.99999, kIuggMeanRadiusM), "m");
  CHECK(ApartM(0.0, 179.99999, 0.0, -179.99999, kIuggMeanRadiusM) < 5.0,
        "**AND TWO POINTS EITHER SIDE OF THE 180TH MERIDIAN ARE NEIGHBOURS.** Subtracting the "
        "longitudes gives 360 degrees and 40 000 km; wrapping the difference gives 2 m, which is "
        "what they are. board:1524's antimeridian stratum exists because this is silent when wrong");

  Network grid(kSnapM, kIuggMeanRadiusM);
  double lat = 48.0, lon = 11.0;
  for (int row = 0; row < kSide; ++row) {
    std::vector<double> along;
    for (int column = 0; column < kSide; ++column) {
      along.push_back(lat + (double)row * kBlockDeg);
      along.push_back(lon + (double)column * kBlockDeg);
    }
    grid.Lay(std::span<const double>(along.data(), 2 * (size_t)kSide), 3.5, 0.06, 2);
  }
  for (int column = 0; column < kSide; ++column) {
    std::vector<double> down;
    for (int row = 0; row < kSide; ++row) {
      down.push_back(lat + (double)row * kBlockDeg);
      down.push_back(lon + (double)column * kBlockDeg);
    }
    grid.Lay(std::span<const double>(down.data(), 2 * (size_t)kSide), 3.5, 0.06, 2);
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
  const Route across = grid.Plan(corner, opposite, 0.0);
  if (!across.Found) { std::printf("REFUSED %s\n", across.Error.c_str()); }
  CHECK(across.Found, "and a route is searched from one corner of it to the other");
  if (!across.Found) { return Report(); }

  const double sideM = ApartM(lat, lon, lat + (double)(kSide - 1) * kBlockDeg, lon,
                              kIuggMeanRadiusM);
  const double acrossM = ApartM(lat + (double)(kSide - 1) * kBlockDeg, lon,
                                lat + (double)(kSide - 1) * kBlockDeg,
                                lon + (double)(kSide - 1) * kBlockDeg, kIuggMeanRadiusM);
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

  Network island(kSnapM, kIuggMeanRadiusM);
  const double first[4] = {48.0, 11.0, 48.0, 11.01};
  const double second[4] = {49.0, 12.0, 49.0, 12.01};
  island.Lay(std::span<const double>(first, 2 * 2), 3.5, 0.06, 2);
  island.Lay(std::span<const double>(second, 2 * 2), 3.5, 0.06, 2);
  CHECK(island.Weave(error), "two ways that never meet still weave");
  const Route broken = island.Plan(Waypoint{48.0, 11.0}, Waypoint{49.0, 12.0}, 0.0);
  CHECK(!broken.Found,
        "**AND A NETWORK IN PIECES IS A NAMED REFUSAL AND NOT AN EMPTY ROUTE.** The search says how "
        "many nodes it could reach, so a caller can tell a disconnected network from a search that "
        "gave up -- which is the difference between a data finding and an engine defect");
  std::printf("REFUSAL %s\n", broken.Error.c_str());

  // board:1784: a way's design minimum radius travels the same road as its width and its
  // gradient, and the node merge has to answer what happens where two classes meet. Width takes
  // the widest, gradient the strictest -- and radius takes the LOOSEST, because a junction where
  // a primary meets a service road really does turn as tightly as the service road, and a bound
  // that refused it would refuse the exit rather than the artefact.
  Network classes(kSnapM, kIuggMeanRadiusM);
  const double wide[6] = {50.0, 10.0, 50.0, 10.01, 50.0, 10.02};
  const double narrow[6] = {50.0, 10.02, 50.0, 10.03, 50.0, 10.04};
  const double none[4] = {50.0, 10.04, 50.0, 10.05};
  classes.Lay(std::span<const double>(wide, 3 * 2), 4.75, 0.06, 2, 400.0);
  classes.Lay(std::span<const double>(narrow, 3 * 2), 3.25, 0.08, 2, 200.0);
  classes.Lay(std::span<const double>(none, 2 * 2), 1.75, 0.12, 1);
  CHECK(classes.Weave(error), "three ways of three classes weave");
  const Route along = classes.Plan(Waypoint{50.0, 10.0}, Waypoint{50.0, 10.05}, 0.0);
  CHECK(along.Found && along.Legs.size() == 6, "and a route runs the length of all three");
  if (along.Found && along.Legs.size() == 6) {
    for (size_t at = 0; at < along.Legs.size(); ++at) {
      std::printf("NOTE leg %zu carries a class minimum of %.1f m\n", at,
                  along.Legs[at].MinRadiusM);
    }
    CHECK(along.Legs[0].MinRadiusM == 400.0 && along.Legs[1].MinRadiusM == 400.0,
          "**A LEG CARRIES THE DESIGN MINIMUM RADIUS OF THE WAY IT STANDS ON**: the number "
          "vegetation.json declares per road class reaches the route, which is the only way the "
          "fit can ever know that a six-metre corner on a primary is a finding (board:1784)");
    CHECK(along.Legs[2].MinRadiusM == 200.0,
          "**AND THE NODE WHERE TWO CLASSES MEET TAKES THE LOOSER OF THEM**: the shared node "
          "belongs to both ways, and a junction bends as tightly as the tightest road that "
          "reaches it -- taking the stricter would refuse every motorway exit in the graph");
    CHECK(along.Legs[4].MinRadiusM == 0.0 && along.Legs[5].MinRadiusM == 0.0,
          "**AND A WAY WITH NO DECLARED MINIMUM CARRIES NONE, AND SPREADS NONE**: the kinds "
          "below tertiary have no fetched design radius, so their nodes bound nothing -- an "
          "absent number stays absent instead of becoming a zero that means 'any corner'");
  }

  // board:1809: the network kept FOUR arrays of way-constant attributes, one entry per POINT,
  // and this session added a fifth. 44 bytes a point of which 28 were the same four scalars
  // repeated, when the values already sat on the Way that owns the range. One index instead.
  {
    Network measured(kSnapM, kIuggMeanRadiusM);
    std::vector<double> laid;
    for (int at = 0; at < 5000; ++at) {
      laid.push_back(50.0 + (double)at * 1.0e-5);
      laid.push_back(10.0);
    }
    measured.Lay(laid, 4.75, 0.06, 2, 400.0);
    Note("points laid", (double)measured.PointCount(), "points");
    Note("bytes the point stream costs", (double)measured.PointStreamBytes(), "bytes");
    Note("bytes it holds", (double)measured.PointStreamHeldBytes(), "bytes");
    Note("bytes a point costs", (double)measured.BytesPerPoint(), "bytes");
    Note("what the growth overshoots by",
         (double)measured.PointStreamHeldBytes() / (double)measured.PointStreamBytes(), "x");
    CHECK(measured.PointCount() == 5000, "five thousand points lay");
    CHECK(measured.BytesPerPoint() <= 24,
          "**A WAY CARRIES ITS CLASS ONCE, AND NOT ONCE PER POINT**: a point is two doubles "
          "and one index into the way that owns it -- 20 bytes. Four way-constant scalars "
          "repeated per point cost 28 more, which was 55 % of the stream, and the values were "
          "already on the Way (board:1809)");
    CHECK(measured.PointStreamHeldBytes() == measured.PointStreamBytes(),
          "and it holds exactly what it costs -- a way declares how many points it lays before "
          "it lays them, so the stream is reserved rather than doubled into");
  }

  // board:1813: the vector tiles this engine reads carry two tag keys -- kind and rail -- so
  // no bridge, tunnel or layer reaches it. What does reach it is OSM's own convention: two ways
  // crossing AT GRADE share a node, two crossing grade-separated do not. That is geometry, it
  // survives the tiling, and it is the reconstruction's whole input.
  {
    Network overpass(kSnapM, kIuggMeanRadiusM);
    const double eastWest[4] = {51.0, 9.99, 51.0, 10.01};
    const double northSouth[4] = {50.99, 10.0, 51.01, 10.0};
    overpass.Lay(std::span<const double>(eastWest, 4), 4.75, 0.06, 2, 400.0);
    overpass.Lay(std::span<const double>(northSouth, 4), 3.25, 0.08, 2, 200.0);
    CHECK(overpass.Weave(error), "two ways that cross without sharing a point weave");

    std::vector<Network::Crossing> crossings;
    const Network::Swept swept = overpass.Crossings(crossings).value();
    const size_t found = swept.Found;
    Note("nodes the two ways make", (double)overpass.NodeCount(), "nodes");
    Note("junctions among them", (double)overpass.JunctionCount(), "nodes");
    Note("places they cross in plan", (double)found, "places");
    if (found > 0) {
      std::printf("NOTE they cross at %.6f, %.6f\n", crossings[0].LatDeg, crossings[0].LonDeg);
    }
    CHECK(found == 1 && overpass.JunctionCount() == 0,
          "**A CROSSING THAT SHARES NO NODE IS A GRADE SEPARATION**: OSM writes an at-grade "
          "crossing as a shared node and a bridge as no node at all, so the absence IS the "
          "information -- and a network that only snaps coincident points cannot see it "
          "(board:1813)");
    if (found == 1) {
      CHECK_NEAR(crossings[0].LatDeg, 51.0, 1.0e-9, "deg",
                 "and the place it names is where the two polylines actually meet");
      CHECK_NEAR(crossings[0].LonDeg, 10.0, 1.0e-9, "deg", "in both axes");
    }
  }

  {
    // the same two ways given a shared point are a junction, and nothing crosses.
    Network atGrade(kSnapM, kIuggMeanRadiusM);
    const double eastWest[6] = {51.0, 9.99, 51.0, 10.0, 51.0, 10.01};
    const double northSouth[6] = {50.99, 10.0, 51.0, 10.0, 51.01, 10.0};
    atGrade.Lay(std::span<const double>(eastWest, 6), 4.75, 0.06, 2, 400.0);
    atGrade.Lay(std::span<const double>(northSouth, 6), 3.25, 0.08, 2, 200.0);
    CHECK(atGrade.Weave(error), "the same two ways with a shared point weave");
    std::vector<Network::Crossing> crossings;
    const Network::Swept swept = atGrade.Crossings(crossings).value();
    const size_t found = swept.Found;
    Note("junctions the shared point makes", (double)atGrade.JunctionCount(), "nodes");
    Note("places they cross in plan", (double)found, "places");
    CHECK(atGrade.JunctionCount() == 1 && found == 0,
          "**AND ONE THAT SHARES A NODE IS A JUNCTION AND NOT A CROSSING**: the two are told "
          "apart by the source's own convention rather than by a tag it does not carry, and a "
          "reconstruction that built a bridge here would put one over a crossroads");
  }

  {
    // two ways that never meet cross nowhere, so the sweep is not simply counting pairs.
    Network apart(kSnapM, kIuggMeanRadiusM);
    const double one[4] = {51.0, 9.99, 51.0, 10.01};
    const double two[4] = {51.5, 9.99, 51.5, 10.01};
    apart.Lay(std::span<const double>(one, 4), 4.75, 0.06, 2, 400.0);
    apart.Lay(std::span<const double>(two, 4), 4.75, 0.06, 2, 400.0);
    CHECK(apart.Weave(error), "two parallel ways weave");
    std::vector<Network::Crossing> crossings;
    Note("places two parallel ways cross", (double)apart.Crossings(crossings).value().Found, "places");
    CHECK(apart.Crossings(crossings).value().Found == 0,
          "and two ways that never meet cross nowhere, so the sweep is finding intersections "
          "rather than counting pairs");
  }

  // board:1822: the sweep pruned by WAY bounding box in raw degrees. A way stepping from
  // 179.99 to -179.99 gets a box spanning the whole planet, so it is a candidate against every
  // way there is and the segment carrying the seam is a 360-degree chord that meets most of
  // them -- while two ways that genuinely cross AT the seam are missed for the same reason.
  {
    Network seam(kSnapM, kIuggMeanRadiusM);
    const double eastWest[4] = {51.0, 179.99, 51.0, -179.99};
    const double northSouth[4] = {50.99, 180.0, 51.01, 180.0};
    seam.Lay(std::span<const double>(eastWest, 4), 4.75, 0.06, 2, 400.0);
    seam.Lay(std::span<const double>(northSouth, 4), 3.25, 0.08, 2, 200.0);
    CHECK(seam.Weave(error), "two ways crossing at the antimeridian weave");
    std::vector<Network::Crossing> crossings;
    const Network::Swept swept = seam.Crossings(crossings).value();
    const size_t found = swept.Found;
    Note("places they cross at the seam", (double)found, "places");
    if (found > 0) {
      std::printf("NOTE they cross at %.6f, %.6f\n", crossings[0].LatDeg, crossings[0].LonDeg);
    }
    CHECK(found == 1,
          "**A CROSSING AT THE ANTIMERIDIAN IS ONE CROSSING**: the longitudes are unrolled "
          "about the first point before any geometry runs, so a way that steps over the seam "
          "is 0.02 degrees long rather than 359.98, and the segment that carries it meets what "
          "it actually meets (board:1822)");
    if (found == 1) {
      CHECK_NEAR(std::fabs(crossings[0].LonDeg), 180.0, 1.0e-6, "deg",
                 "and the place it names is on the seam, reported back inside [-180, 180]");
      CHECK_NEAR(crossings[0].LatDeg, 51.0, 1.0e-6, "deg", "at the latitude the ways meet");
    }
  }

  {
    Network sweep(kSnapM, kIuggMeanRadiusM);
    std::vector<double> running;
    constexpr int kWays = 60;
    for (int one = 0; one < kWays; ++one) {
      running.clear();
      for (int step = 0; step <= 200; ++step) {
        running.push_back(51.0 + 0.001 * (double)one);
        running.push_back(9.9995 + 0.001 * (double)step);
      }
      sweep.Lay(running, 4.75, 0.06, 2, 400.0);
      running.clear();
      for (int step = 0; step <= 200; ++step) {
        running.push_back(50.9995 + 0.001 * (double)step);
        running.push_back(10.0 + 0.001 * (double)one);
      }
      sweep.Lay(running, 3.25, 0.08, 2, 200.0);
    }
    CHECK(sweep.Weave(error), "a grid of long ways weaves");
    std::vector<Network::Crossing> crossings;
    const Network::Swept swept = sweep.Crossings(crossings).value();
    const size_t found = swept.Found;
    const size_t segments = sweep.PointCount() - 2u * (size_t)kWays;
    Note("ways laid", (double)(2 * kWays), "ways");
    Note("segments among them", (double)segments, "segments");
    Note("crossings found", (double)found, "crossings");
    Note("segment pairs tested", (double)swept.PairsTested, "pairs");
    Note("the fullest cell", (double)swept.FullestCell, "segments");
    Note("pairs tested per crossing found",
         found > 0 ? (double)swept.PairsTested / (double)found : 0.0, "pairs");
    Note("what testing every pair would have cost",
         (double)segments * (double)segments * 0.5, "pairs");
    CHECK(found == (size_t)(kWays * kWays),
          "**AND A GRID OF LONG WAYS FINDS EVERY ONE OF ITS CROSSINGS** -- 60 by 60 ways "
          "crossing nowhere near a shared node is 3600 grade separations");
    // board:1850: the grid's entry became one 8-byte record where it was 4 + 8 + 8 across
    // three arrays. Found and PairsTested are geometry -- a square is a square whatever indexes
    // it -- so both must be untouched by the layout change. FullestCell is bucket occupancy and
    // therefore a property of the HASH: it moved 30 -> 16 on this fixture, which is the new
    // mixer spreading better, not a different answer.
    CHECK(found == 3600u && swept.PairsTested == 86284u,
          "**AND THE ANSWER AND ITS COST SURVIVE A CHANGE OF LAYOUT**: the square a segment "
          "falls in is geometric, so packing (x, y) into one index may move which BUCKET holds "
          "it and may not move which pairs share a square (board:1850)");
    CHECK(swept.FullestCell <= 16u,
          "and the fullest bucket is no worse than the three-array grid's, measured: 30 "
          "segments before, 16 after");
    CHECK(swept.PairsTested < segments * 20u,
          "**AND THE SEARCH IS OVER SEGMENTS IN A GRID, NOT OVER WAY BOXES**: a motorway way "
          "carries hundreds of points and a box the length of the country it crosses, so a "
          "way-box prune does nothing for exactly the ways this reconstruction is about -- the "
          "cost per segment is bounded by what shares its cell (board:1822)");
  }

  // board:1831: the cell size used to come from the bounding box AREA, so a network with no
  // extent in latitude -- every way exactly east-west, which is what a fixture lays -- took a
  // 1.0e-12 clamp and produced 141 422 cells across for two segments, or 7 million over ten
  // degrees. The size comes from the SEGMENTS now, and the table is a hash of segment count.
  {
    Network flat(kSnapM, kIuggMeanRadiusM);
    std::vector<double> eastward;
    for (int one = 0; one < 8; ++one) {
      eastward.clear();
      for (int step = 0; step <= 20; ++step) {
        eastward.push_back(51.0);
        eastward.push_back(10.0 + 0.4 * (double)one + 0.02 * (double)step);
      }
      flat.Lay(eastward, 4.75, 0.06, 2, 400.0);
    }
    CHECK(flat.Weave(error), "eight ways at ONE latitude weave -- the bbox has no height at all");
    std::vector<Network::Crossing> crossings;
    const Network::Swept swept = flat.Crossings(crossings).value();
    const size_t found = swept.Found;
    Note("ways at one latitude", 8.0, "ways");
    Note("crossings among them", (double)found, "crossings");
    Note("pairs tested", (double)swept.PairsTested, "pairs");
    Note("the fullest cell", (double)swept.FullestCell, "segments");
    CHECK(swept.PairsTested < 8u * 20u * 20u,
          "**A NETWORK WITH NO EXTENT IN ONE AXIS COSTS WHAT ITS SEGMENTS COST**, not what a "
          "degenerate bounding box divides into: the cell size comes from the segment lengths "
          "the network holds, so a flat network cannot produce a cell table larger than the "
          "segments in it (board:1831)");
  }

  {
    Network clustered(kSnapM, kIuggMeanRadiusM);
    std::vector<double> tight;
    for (int one = 0; one < 40; ++one) {
      tight.clear();
      for (int step = 0; step <= 20; ++step) {
        tight.push_back(51.0 + 0.00002 * (double)one);
        tight.push_back(10.0 + 0.00005 * (double)step);
      }
      clustered.Lay(tight, 4.75, 0.06, 2, 400.0);
    }
    const double faraway[4] = {56.0, 15.0, 56.001, 15.001};
    clustered.Lay(std::span<const double>(faraway, 4), 4.75, 0.06, 2, 400.0);
    CHECK(clustered.Weave(error),
          "forty ways inside a thousandth of a degree, plus one way five degrees away, weave");
    std::vector<Network::Crossing> crossings;
    const Network::Swept swept = clustered.Crossings(crossings).value();
    const size_t found = swept.Found;
    const size_t segments = 40u * 20u + 1u;
    Note("segments in the cluster and its outlier", (double)segments, "segments");
    Note("crossings found", (double)found, "crossings");
    Note("pairs tested", (double)swept.PairsTested, "pairs");
    Note("the fullest cell", (double)swept.FullestCell, "segments");
    CHECK(swept.FullestCell < segments,
          "**AND A CLUSTER WITH ONE OUTLIER DOES NOT PUT EVERY SEGMENT IN ONE CELL**: a cell "
          "size scaled to the whole bounding box would, and the pair loop inside that cell is "
          "O(n^2) with nothing saying so -- the fullest cell is published beside the total so a "
          "clustered network is a number rather than a slow run (board:1831)");
  }

  // board:1835: the hash grid's dedup compared the BUCKET a crossing falls in, not the SQUARE.
  // Two squares that collide in one bucket then accept the same pair once per copy, so a single
  // crossing is reported as many times as the longer segment's box has squares. The dense grid
  // could not do this -- its cell index was injective -- so replacing it with a hash replaced a
  // property nothing was asserting.
  {
    Network once(kSnapM, kIuggMeanRadiusM);
    const double northSouth[4] = {50.85, 10.00005, 51.15, 10.00005};
    once.Lay(std::span<const double>(northSouth, 4), 4.0, 0.06, 2, 400.0);
    std::vector<double> eastWest;
    for (int step = 0; step <= 60; ++step) {
      eastWest.push_back(51.0);
      eastWest.push_back(9.99 + 0.02 * (double)step / 60.0);
    }
    once.Lay(eastWest, 4.0, 0.06, 2, 400.0);
    CHECK(once.Weave(error), "one long way crossing sixty short ones weaves");

    std::vector<Network::Crossing> crossings;
    const Network::Swept swept = once.Crossings(crossings).value();
    Note("segments in it", 61.0, "segments");
    Note("crossings it reports", (double)swept.Found, "crossings");
    Note("pairs it tested", (double)swept.PairsTested, "pairs");
    Note("the fullest cell", (double)swept.FullestCell, "segments");
    for (size_t at = 0; at < crossings.size() && at < 3; ++at) {
      std::printf("NOTE crossing %zu at %.7f, %.7f\n", at, crossings[at].LatDeg,
                  crossings[at].LonDeg);
    }

    CHECK(swept.Found == 1,
          "**A CROSSING IS COUNTED ONCE**: two ways that meet at one place are one grade "
          "separation, and a segment whose box spans several squares of a HASH grid lands in "
          "the same bucket more than once -- so a dedup that compares the bucket accepts the "
          "same pair once per copy. This fixture reported it NINE times (board:1835)");
    if (swept.Found == 1) {
      CHECK_NEAR(crossings.front().LatDeg, 51.0, 1.0e-9, "deg",
                 "and the one it reports is where the two ways actually meet");
    }
  }

  {
    Network scattered(6378137.0, 1.0);
    std::string refusal;
    for (int hair = 0; hair < 6; ++hair) {
      std::vector<double> ends = {-80.0 + 32.0 * (double)hair, -170.0 + 68.0 * (double)hair,
                                  -80.0 + 32.0 * (double)hair + 1.0e-7,
                                  -170.0 + 68.0 * (double)hair + 1.0e-7};
      scattered.Lay(ends, 3.5, 0.0, 2, 100.0);
    }
    CHECK(scattered.Weave(refusal), "six hair-length ways scattered over the sphere weave");
    std::vector<Network::Crossing> crossings;
    const auto sweep = scattered.Crossings(crossings);
    std::printf("NOTE the sweep says: '%.100s'\n",
                sweep.has_value() ? "(it gridded them)" : std::string(sweep.error()).c_str());
    CHECK(!sweep.has_value(),
          "**AND A GRID THAT WILL NOT FIT ITS DECLARED WIDTH REFUSES**: the cell size is twice "
          "the MEAN segment reach, so a network of hair-length ways spread over a sphere asks "
          "for more squares than a 32-bit index holds -- the old grid cast to platform-width "
          "`long` and wrapped in silence (board:1850)");
  }

  Covers("I.96 a network is woven from ways that share no identity: points within a declared "
         "snapping distance become one node, ways meet where they cross, and A* over it with a "
         "great-circle heuristic returns the shortest route or names why there is none");
  return Report();
}
