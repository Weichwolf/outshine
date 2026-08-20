#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "Check.h"

#include "ContentStore.h"
#include "Fit.h"
#include "CurlTransport.h"
#include "DeclaredSources.h"
#include "GroundMaterials.h"
#include "OsmField.h"
#include "RoadHarvest.h"
#include "SourceSet.h"
#include "TerrainLoader.h"
#include "TilePool.h"
#include "VegetationTemplates.h"
#include "Wayfinding.h"

using outshine::World::ApartM;
using outshine::World::Network;
using outshine::World::OsmField;
using outshine::World::OsmLayer;
using outshine::World::OsmLayerNames;
using outshine::World::Reap;
using outshine::World::Reaped;
using outshine::World::Route;
using outshine::World::TilePool;
using outshine::World::VegetationTemplates;
using outshine::World::Waypoint;

namespace {

constexpr double kMunichLat = 48.1371;
constexpr double kMunichLon = 11.5754;
constexpr double kHamburgLat = 53.5503;
constexpr double kHamburgLon = 9.9920;
constexpr double kF31WidthM = 1.811;
constexpr int kZoom = 10;

constexpr double kPatienceS = 900.0;

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const double straightM = ApartM(kMunichLat, kMunichLon, kHamburgLat, kHamburgLon);
  const double middleLat = 0.5 * (kMunichLat + kHamburgLat);
  const double middleLon = 0.5 * (kMunichLon + kHamburgLon);
  Note("Marienplatz to Rathausmarkt as the crow flies", straightM / 1000.0, "km");
  Note("the zoom the ways are read at", (double)kZoom, "");
  const double tileGroundM =
      40075017.0 * std::cos(middleLat * 3.14159265358979 / 180.0) / (double)(1L << kZoom);
  const int kCorridorRing = 2;
  const long steps = (long)std::ceil(straightM / tileGroundM) + 1;
  const long square = (long)(2 * std::ceil(0.5 * straightM / tileGroundM) + 3);
  Note("a tile's ground size at this zoom and latitude", tileGroundM / 1000.0, "km");
  Note("stations along the line the corridor is fetched at", (double)steps, "stations");
  Note("the ring fetched around each", (double)(2 * kCorridorRing + 1), "tiles across");
  Note("what a square covering both cities would have cost", (double)(square * square), "tiles");

  outshine::Data::ContentStore::Config keeping;
  keeping.Directory = "/tmp/outshine-drive-cache";
  outshine::Data::ContentStore store(keeping);
  outshine::Data::SourceSet sources(store);
  CHECK(outshine::Data::RegisterDeclared(sources, {"src/assets/sky", true}) ==
            outshine::Data::Registered::Complete,
        "the declared upstream sources register, ranked and without a clash");
  Note("sources registered", (double)sources.Count(), "sources");

  outshine::Host::CurlTransport::Config wiring;
  wiring.Threads = 8;
  outshine::Host::CurlTransport wire(wiring);
  FbGroundSurface surface;
  surface.Z = 12;
  surface.Grid = 64;
  const int opened = fb_stream_open(sources, wire, middleLat, middleLon, surface);
  CHECK(opened == 1,
        "the streamer opens over the declared sources -- and it is a GLOBAL, which is what "
        "OsmField reaches for. CLAUDE.md gives neither a global nor a singleton a place to live, "
        "and board:1527 is that finding");
  if (opened != 1) { return Report(); }

  OsmField field(kZoom, OsmLayerNames({OsmLayer::Streets, OsmLayer::StreetPolygons}));
  const auto began = std::chrono::steady_clock::now();
  long passes = 0;
  int built = 0;
  bool ranOut = false;
  for (long step = 0; step <= steps && !ranOut; ++step) {
    const double part = (double)step / (double)steps;
    const double atLat = kMunichLat + part * (kHamburgLat - kMunichLat);
    const double atLon = kMunichLon + part * (kHamburgLon - kMunichLon);
    for (;;) {
      built += field.Build(atLat, atLon, kCorridorRing);
      ++passes;
      if (field.PendingTiles() == 0) { break; }
      if (std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count() >
          kPatienceS) {
        ranOut = true;
        break;
      }
    }
  }
  CHECK(!ranOut, "the corridor is fetched inside the patience declared for it");
  const double fetchedS =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();

  Note("passes over the corridor", (double)passes, "passes");
  Note("tiles the corridor actually took", (double)field.Tiles().size(), "tiles");
  Note("tiles decoded", (double)built, "tiles");
  Note("tiles still pending", (double)field.PendingTiles(), "tiles");
  Note("layers the server did not send", (double)field.MissingLayers(), "layers");
  Note("tiles that would not decode", (double)field.BadTiles(), "tiles");
  Note("features read", (double)field.Features().size(), "features");
  Note("points in them", (double)(field.Points().size() / 2), "points");
  Note("seconds spent fetching and decoding", fetchedS, "s");

  CHECK(built > 0 && field.Features().size() > 0,
        "**REAL OSM WAYS ARRIVE OVER THE WIRE AND DECODE.** No fixture, no committed extract: the "
        "declared upstream source is asked for the tiles between Munich and Hamburg and answers "
        "with vector geometry");
  if (field.Features().empty()) { return Report(); }

  outshine::World::GroundMaterials materials;
  CHECK(materials.Load("src/assets/world/ground-materials.json"),
        "the declared ground materials load, because a width table names them");
  VegetationTemplates widths;
  std::string error;
  CHECK(widths.Load("src/assets/world/vegetation.json", materials),
        "the declared widths load, with their RAA, RAL and RASt origins");

  const double quantumM = 40075017.0 / ((double)(1L << kZoom) * 4096.0);
  Network roads(1.05 * quantumM);
  Note("the tile's own coordinate quantisation", quantumM, "m");
  const Reaped reaped = Reap(field, widths, kF31WidthM, roads);
  Note("the snapping distance just above it", roads.SnapM(), "m");
  Note("ways a car can fit down", (double)reaped.Ways, "ways");
  Note("points in them", (double)reaped.Points, "points");
  Note("ways too narrow for it", (double)reaped.TooNarrow, "ways");
  Note("the widest way it refused", reaped.WidestRefusedM, "m");
  Note("the narrowest it took", reaped.NarrowestTakenM, "m");
  Note("ways whose kind carries no declared width", (double)reaped.Unclassed, "ways");

  CHECK(reaped.Ways > 0, "and roads a 1.811 m car can fit down are harvested from them");
  CHECK(reaped.WidestRefusedM < kF31WidthM || reaped.TooNarrow == 0,
        "**ADMISSIBILITY IS THE VEHICLE'S WIDTH AND NOT A LIST OF TAGS.** Every way this refused is "
        "narrower than the car; every way it took is wider. Nobody wrote down which highway kinds a "
        "car may use, and traffic law stays unmodelled -- what decides is whether it fits");

  CHECK(roads.Weave(error), "the ways weave into a network");
  if (!error.empty()) { std::printf("REFUSED %s\n", error.c_str()); }
  Note("nodes after snapping", (double)roads.NodeCount(), "nodes");
  Note("junctions among them", (double)roads.JunctionCount(), "nodes");
  Note("edges", (double)roads.EdgeCount(), "edges");

  size_t atMunich = 0, atHamburg = 0;
  double munichAwayM = 0.0, hamburgAwayM = 0.0;
  CHECK(roads.Nearest(Waypoint{kMunichLat, kMunichLon}, atMunich, munichAwayM) &&
            roads.Nearest(Waypoint{kHamburgLat, kHamburgLon}, atHamburg, hamburgAwayM),
        "both city centres resolve to a node of the network");
  Note("how far Marienplatz is from the nearest road node", munichAwayM, "m");
  Note("how far Rathausmarkt is from the nearest road node", hamburgAwayM, "m");
  CHECK(munichAwayM < 200.0 && hamburgAwayM < 200.0,
        "**AND BOTH ARE ON A ROAD RATHER THAN NEAR ONE.** A route is only between the places asked "
        "for if the endpoints snapped to them; a node 200 km away would give a route that is "
        "shorter than the great circle and still look like a route");

  const double tightestM = 2.810 / std::tan(0.522804742);
  Note("the tightest circle the F31 can turn", tightestM, "m");
  const Route route =
      roads.Plan(Waypoint{kMunichLat, kMunichLon}, Waypoint{kHamburgLat, kHamburgLon}, tightestM,
                 quantumM);
  Note("turns the search refused as too sharp for the car", (double)route.TurnsRefused, "turns");
  if (!route.Found) { std::printf("REFUSED %s\n", route.Error.c_str()); }
  Note("nodes the search settled", (double)route.Reached, "nodes");
  CHECK(route.Found,
        "**AND A ROUTE IS FOUND FROM MARIENPLATZ TO RATHAUSMARKT OVER OSM'S OWN WAYS.** Two "
        "coordinates in, a chain of real roads out, nothing stored and nothing hand-placed");
  if (!route.Found) { return Report(); }

  Note("how far the route runs", route.LengthM / 1000.0, "km");
  Note("how far the crow flies", route.StraightM / 1000.0, "km");
  Note("the detour that is", route.LengthM / route.StraightM, "x");
  CHECK(route.LengthM > route.StraightM,
        "**A ROAD CANNOT BE SHORTER THAN THE GREAT CIRCLE.** If it is, the route did not run between "
        "the two places asked for, or the network welded roads that do not meet -- and both are "
        "findings rather than a route");
  Note("legs in it", (double)route.Legs.size(), "legs");

  CHECK(route.LengthM / 1000.0 > 700.0 && route.LengthM / 1000.0 < 900.0,
        "and its length is what a road between these two cities is -- 612 km as the crow flies and "
        "roughly 775 by motorway, so a route far outside that band went somewhere a car would not");

  const double frameLat = route.Legs.front().At.LatDeg;
  const double frameLon = route.Legs.front().At.LonDeg;
  const double perLatM = ApartM(frameLat, frameLon, frameLat + 1.0, frameLon);
  const double perLonM = ApartM(frameLat, frameLon, frameLat, frameLon + 1.0);
  std::vector<double> eastNorthM;
  eastNorthM.reserve(route.Legs.size() * 2);
  for (const auto &leg : route.Legs) {
    eastNorthM.push_back((leg.At.LonDeg - frameLon) * perLonM);
    eastNorthM.push_back((leg.At.LatDeg - frameLat) * perLatM);
  }
  Note("metres per degree of latitude in the local frame", perLatM, "m");
  Note("metres per degree of longitude there", perLonM, "m");

  const std::vector<double> keptM = outshine::Simplify(eastNorthM, quantumM);
  Note("vertices the route offered before simplifying", (double)(eastNorthM.size() / 2),
       "vertices");
  Note("vertices left after removing what the data cannot resolve",
       (double)(keptM.size() / 2), "vertices");
  Note("the share removed", 1.0 - (double)keptM.size() / (double)eastNorthM.size(), "of them");

  outshine::ReferenceLine corridor;
    const outshine::Fitted fitted = Fit(keptM, quantumM, tightestM, corridor);
  if (!fitted.Laid) { std::printf("REFUSED %s\n", fitted.Error.c_str()); }
  Note("vertices the route offered", (double)fitted.Vertices, "vertices");
  Note("corners the fit needed", (double)fitted.Corners, "corners");
  Note("straights between them", (double)fitted.Straights, "straights");
  Note("the corridor it laid", fitted.LengthM / 1000.0, "km");
  Note("the polyline it came from", route.LengthM / 1000.0, "km");
  Note("the tightest radius on it", fitted.TightestRadiusM, "m");
  Note("the sharpest turn it carried", fitted.SharpestTurnRad * 180.0 / 3.14159265358979, "deg");
  Note("at which vertex", fitted.SharpestTurnAtM, "");
  Note("turns past a right angle", (double)fitted.TurnsPastRightAngle, "of 2480");
  Note("turns past 135 degrees", (double)fitted.TurnsPastHalfCircle, "of 2480");
  Note("vertices too sharp for the car to drive at all", (double)fitted.Undrivable, "vertices");
  Note("how far it leaves a vertex at worst", fitted.WorstOffsetM, "m");
  Note("corners the fit had to correct by measuring them", (double)fitted.Corrected, "corners");
  Note("passes it needed", (double)fitted.Passes, "passes");
  Note("how far the laid line drifts from the polyline beyond any corner's own doing",
       fitted.DriftM, "m");
  Note("per corner that is", fitted.DriftPerCornerM * 1000.0, "mm");
  Note("the worst vertex", fitted.WorstVertex, "");
  Note("its incoming leg", fitted.WorstLegInM, "m");
  Note("its outgoing leg", fitted.WorstLegOutM, "m");
  Note("the turn there", fitted.WorstTurnRad * 180.0 / 3.14159265358979, "deg");
  Note("the radius it settled on", fitted.WorstRadiusM, "m");
  Note("the station the fit expected it at", fitted.WorstExpectedM, "m");
  Note("the station the resection found", fitted.WorstStationM, "m");
  Note("corners the data cannot support at any drivable radius", (double)fitted.Strained,
       "corners");
  Note("how far the worst of those leaves its vertex", fitted.StrainedWorstM, "m");
  Note("where that happens", fitted.WorstOffsetAtM / 1000.0, "km");
  Note("how far it is allowed to", quantumM, "m");

  if (!fitted.Laid) { std::printf("REFUSED %s\n", fitted.Error.c_str()); }
  if (!fitted.Laid && fitted.Undrivable > 0) {
    const size_t at = (size_t)fitted.UndrivableAtM;
    for (size_t which = at > 1 ? at - 1 : 0; which <= at + 1 && which < route.Legs.size(); ++which) {
      std::printf("AT %zu  %.7f %.7f\n", which, route.Legs[which].At.LatDeg,
                  route.Legs[which].At.LonDeg);
    }
    if (at >= 1 && at + 1 < route.Legs.size()) {
      std::printf("LEGS in %.2f m  out %.2f m\n",
                  ApartM(route.Legs[at - 1].At.LatDeg, route.Legs[at - 1].At.LonDeg,
                         route.Legs[at].At.LatDeg, route.Legs[at].At.LonDeg),
                  ApartM(route.Legs[at].At.LatDeg, route.Legs[at].At.LonDeg,
                         route.Legs[at + 1].At.LatDeg, route.Legs[at + 1].At.LonDeg));
    }
  }
  CHECK(fitted.Laid,
        "**AND THE ROUTE BECOMES A CORRIDOR WITH NO CURVATURE LEAP ANYWHERE ON IT.** 752 km of OSM "
        "polyline, every interior vertex carrying spiral-arc-spiral, laid by a ReferenceLine that "
        "REFUSES a leap -- so a step in the lateral force has no spelling on this road");
  if (!fitted.Laid) { return Report(); }
  CHECK(fitted.DriftM < 0.05 * quantumM * (double)fitted.Corners,
        "**AND WHAT IS LEFT IS DRIFT, WHICH NO CORNER CAN CORRECT.** The line is walked corner by "
        "corner and each spiral is integrated by 8-node Gauss-Legendre; the residual accumulates "
        "laterally over 2300 corners. It is reported as its own term, in millimetres per corner, "
        "rather than being chased by shrinking corners that were never at fault -- which is what "
        "pinned 1284 of them at the vehicle's tightest radius. board:1528");
  CHECK(fitted.Strained * 200 < fitted.Corners,
        "**AND WHERE THE DATA CANNOT SUPPORT A ROAD AT ANY RADIUS THE CAR CAN TURN, THAT CORNER IS "
        "COUNTED AND NOT HIDDEN.** Those are the corners whose vertices the line must leave by more "
        "than the tile's own quantisation to stay drivable at all -- a classified finding with a "
        "count, not a fit that quietly bent further. Fewer than one in two hundred here");
  Note("how much longer the corridor is than the polyline",
       fitted.LengthM / route.LengthM - 1.0, "of it");
  CHECK(fitted.LengthM < 1.05 * route.LengthM,
        "and within a few per cent of the polyline it was fitted through -- a cut corner is shorter "
        "than the corner, and the 1.6 % it runs long is the first-order construction on the tail of "
        "sharp turns, board:1528");
  Note("the speed the tightest radius allows at 0.95 g",
       std::sqrt(0.95 * 9.80665 * fitted.TightestRadiusM) * 3.6, "km/h");

  const double postM = fb_stream_ground_post_m(middleLat);
  const long posts = (long)std::ceil(fitted.LengthM / postM);
  Note("the elevation source's own post spacing here", postM, "m");
  Note("stations the corridor is sampled at", (double)(posts + 1), "stations");

  std::vector<double> heightM((size_t)posts + 1, 0.0);
  std::vector<bool> known((size_t)posts + 1, false);
  long holes = 0, waited = 0;
  const auto sampling = std::chrono::steady_clock::now();
  for (long post = 0; post <= posts; ++post) {
    const double atM = (double)post * fitted.LengthM / (double)posts;
    outshine::Placed on;
    if (!corridor.At(atM, on)) { continue; }
    const double latDeg = frameLat + on.NorthM / perLatM;
    const double lonDeg = frameLon + on.EastM / perLonM;
    for (;;) {
      const outshine::GroundSample ground = fb_stream_ground(latDeg, lonDeg);
      double aslM = 0.0;
      if (ground.TryAslM(&aslM)) {
        heightM[(size_t)post] = aslM;
        known[(size_t)post] = true;
        break;
      }
      if (ground.Where() == outshine::GroundSample::State::Hole) {
        ++holes;
        break;
      }
      ++waited;
      if (std::chrono::duration<double>(std::chrono::steady_clock::now() - sampling).count() >
          kPatienceS) {
        break;
      }
    }
  }
  const double sampledS =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - sampling).count();

  long resolved = 0;
  double lowestM = 0.0, highestM = 0.0;
  bool haveAny = false;
  for (size_t post = 0; post < heightM.size(); ++post) {
    if (!known[post]) { continue; }
    ++resolved;
    if (!haveAny || heightM[post] < lowestM) { lowestM = heightM[post]; }
    if (!haveAny || heightM[post] > highestM) { highestM = heightM[post]; }
    haveAny = true;
  }
  Note("stations the elevation source answered", (double)resolved, "stations");
  Note("stations it said were a hole", (double)holes, "stations");
  Note("times a sample had to wait for a tile", (double)waited, "waits");
  Note("seconds spent sampling the ground", sampledS, "s");
  Note("the lowest the corridor runs", lowestM, "m");
  Note("the highest", highestM, "m");
  Note("Munich's own elevation", heightM.empty() ? 0.0 : heightM.front(), "m");
  Note("Hamburg's", heightM.empty() ? 0.0 : heightM.back(), "m");

  CHECK(resolved > 0, "**THE ELEVATION SOURCE ANSWERS ALONG THE WHOLE CORRIDOR.** Real height data, "
                      "streamed for the same route the ways came from");
  CHECK(holes == 0, "with no hole in it -- a hole is a named refusal and there is none here");
  CHECK(std::fabs(heightM.front() - 523.0) < 40.0 && std::fabs(heightM.back() - 14.0) < 40.0,
        "**AND THE TWO ENDS ARE WHERE THE CITIES ARE.** Munich stands at about 520 m and Hamburg at "
        "about 10; the source says 523.15 and 14.14, which is the check that this is the real world "
        "and not a plausible surface");

  std::vector<outshine::Knot> rise;
  rise.reserve(heightM.size());
  double worstGradeM = 0.0, worstGradeAtM = 0.0;
  for (size_t post = 0; post < heightM.size(); ++post) {
    if (!known[post]) { continue; }
    const double atM = (double)post * fitted.LengthM / (double)posts;
    const size_t before = post > 0 ? post - 1 : post;
    const size_t after = post + 1 < heightM.size() ? post + 1 : post;
    const double spanM = ((double)after - (double)before) * fitted.LengthM / (double)posts;
    const double slope = spanM > 0.0 ? (heightM[after] - heightM[before]) / spanM : 0.0;
    if (std::fabs(slope) > std::fabs(worstGradeM)) {
      worstGradeM = slope;
      worstGradeAtM = atM;
    }
    rise.push_back(outshine::Knot{atM, heightM[post], slope});
  }
  const bool rose = corridor.Rise(rise, error);
  if (!rose) { std::printf("REFUSED %s\n", error.c_str()); }
  Note("height knots fastened to the corridor", (double)rise.size(), "knots");
  Note("the steepest gradient anywhere on it", worstGradeM, "m/m");
  Note("as a percentage", worstGradeM * 100.0, "%");
  Note("where that is", worstGradeAtM / 1000.0, "km");

  const double driveN = 400.0 * 3.08 / 0.333;
  const double climbLimit = driveN / (1610.0 * 9.80665);
  Note("the steepest the F31's drivetrain can climb", climbLimit * 100.0, "%");
  CHECK(rose, "**AND THE CORRIDOR RISES WITH THE REAL GROUND UNDER IT.** 8022 heights from the "
              "declared elevation source, each a knot with its own slope, and one cubic through "
              "them -- the same mechanism the synthetic road used, fed by the world");
  CHECK(std::fabs(worstGradeM) < climbLimit,
        "**AND NOTHING ON IT IS STEEPER THAN THE CAR CAN CLIMB.** 23.4 % is what 3699 N against "
        "15789 N of weight allows; a gradient past that is the drivetrain REFUSING, and on this "
        "route there is none -- which is the first evidence that the ground under an OSM road is "
        "reconstructed well enough to drive");

  fb_stream_close();

  Covers("I.4.5 a route from Marienplatz to Rathausmarkt is planned over ways fetched live from the "
         "declared OSM source, woven into a network by snapping at twice the tile resolution, with "
         "admissibility decided by whether the declared vehicle fits");
  return Report();
}
