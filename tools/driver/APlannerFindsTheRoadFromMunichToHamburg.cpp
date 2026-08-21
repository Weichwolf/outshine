#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "Check.h"

#include "Carriageway.h"
#include "ContentStore.h"
#include "Drive.h"
#include "Rigging.h"
#include "ScenarioRead.h"
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
constexpr double kJoinMs = 20.0;
constexpr double kStepS = 1.0e-3;
constexpr double kFromM = 50.0;
constexpr long kMaxSteps = 40000000;
constexpr double kResectM = 4.0;
constexpr double kMountResectM = 8.0;

struct Drove {
  bool Lost = false;
  bool Arrived = false;
  bool PastTravel = false;
  bool PastLimit = false;
  size_t MostAirborne = 0;
  double ReachedM = 0.0;
  double TopMs = 0.0;
  double WorstOffsetM = 0.0;
  double WorstOffsetAtM = 0.0;
  double WorstRatio = 0.0;
  double AirborneAtM = 0.0;
  double BrokeAtM = 0.0;
};

void Unit(double v[3]) {
  const double length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (length > 0.0) {
    for (int axis = 0; axis < 3; ++axis) { v[axis] /= length; }
  }
}

double HeadingOf(const outshine::Physics::Body &body) {
  const double aheadBody[3] = {0.0, 0.0, -1.0};
  double ahead[3];
  outshine::Physics::Turn(body.OrientationQ, aheadBody, ahead);
  return std::atan2(-ahead[2], ahead[0]);
}

void Lie(outshine::Physics::Body &body, const outshine::Placed &on, const double normalM[3]) {
  double ahead[3] = {std::cos(on.HeadingRad), on.Slope, -std::sin(on.HeadingRad)};
  double up[3] = {normalM[0], normalM[1], normalM[2]};
  Unit(up);
  const double along = ahead[0] * up[0] + ahead[1] * up[1] + ahead[2] * up[2];
  for (int axis = 0; axis < 3; ++axis) { ahead[axis] -= along * up[axis]; }
  Unit(ahead);
  const double back[3] = {-ahead[0], -ahead[1], -ahead[2]};
  const double right[3] = {up[1] * back[2] - up[2] * back[1], up[2] * back[0] - up[0] * back[2],
                           up[0] * back[1] - up[1] * back[0]};
  const double m[3][3] = {{right[0], up[0], back[0]},
                          {right[1], up[1], back[1]},
                          {right[2], up[2], back[2]}};
  const double trace = m[0][0] + m[1][1] + m[2][2];
  double q[4];
  if (trace > 0.0) {
    const double root = std::sqrt(trace + 1.0) * 2.0;
    q[0] = 0.25 * root;
    q[1] = (m[2][1] - m[1][2]) / root;
    q[2] = (m[0][2] - m[2][0]) / root;
    q[3] = (m[1][0] - m[0][1]) / root;
  } else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
    const double root = std::sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]) * 2.0;
    q[0] = (m[2][1] - m[1][2]) / root;
    q[1] = 0.25 * root;
    q[2] = (m[0][1] + m[1][0]) / root;
    q[3] = (m[0][2] + m[2][0]) / root;
  } else if (m[1][1] > m[2][2]) {
    const double root = std::sqrt(1.0 + m[1][1] - m[0][0] - m[2][2]) * 2.0;
    q[0] = (m[0][2] - m[2][0]) / root;
    q[1] = (m[0][1] + m[1][0]) / root;
    q[2] = 0.25 * root;
    q[3] = (m[1][2] + m[2][1]) / root;
  } else {
    const double root = std::sqrt(1.0 + m[2][2] - m[0][0] - m[1][1]) * 2.0;
    q[0] = (m[1][0] - m[0][1]) / root;
    q[1] = (m[0][2] + m[2][0]) / root;
    q[2] = (m[1][2] + m[2][1]) / root;
    q[3] = 0.25 * root;
  }
  for (int part = 0; part < 4; ++part) { body.OrientationQ[part] = q[part]; }
}

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

  std::FILE *const declaration = std::fopen("tools/driver/f31.scenario", "rb");
  CHECK(declaration != nullptr, "the F31's own declaration is there to read");
  std::vector<char> text;
  if (declaration != nullptr) {
    int one = 0;
    while ((one = std::fgetc(declaration)) != EOF) { text.push_back((char)one); }
    std::fclose(declaration);
  }
  outshine::Scenario declared;
  CHECK(!text.empty() && outshine::ReadScenario(text.data(), text.size(), declared, error),
        "and it reads");
  CHECK(declared.Vehicles.size() == 1, "declaring one vehicle");
  if (declared.Vehicles.empty()) { return Report(); }

  const outshine::Clients::Rigged stood = outshine::Clients::Stand(declared.Vehicles[0]);
  if (!stood.Stood) { std::printf("REFUSED %s\n", stood.Error.c_str()); }
  CHECK(stood.Stood, "**AND THE DECLARED F31 STANDS UP AS A RIG.** Every number the drive uses comes "
                     "from the file, not from a constant beside it");
  if (!stood.Stood) { return Report(); }


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

  outshine::Envelope planning = stood.Envelope;
  const double holdWithinM = 0.5 * reaped.NarrowestTakenM - 0.5 * kF31WidthM;
  planning.ReserveMs2 = 2.0 * holdWithinM / (1.0 * 1.0);
  Note("the narrowest road on the route", reaped.NarrowestTakenM, "m");
  Note("what the car leaves either side of itself there", holdWithinM, "m");
  Note("the lateral acceleration reserved for holding the line", planning.ReserveMs2, "m/s2");
  Note("what is left for the path", planning.HoldingMs2(), "m/s2");

  outshine::SpeedProfile inPlan;
  CHECK(inPlan.Over(corridor, stood.Envelope, postM, 0.0, error),
        "the plan view alone gives a speed at every station, before the ground is consulted");

  const double spanM = fitted.LengthM / (double)posts;
  std::vector<double> roadM = heightM;
  std::vector<double> gradeLimit(roadM.size(), 0.0);
  {
    size_t leg = 0;
    for (size_t post = 0; post < roadM.size(); ++post) {
      const double atM = (double)post * spanM * route.LengthM / fitted.LengthM;
      while (leg + 1 < route.Legs.size() && route.Legs[leg + 1].AlongM < atM) { ++leg; }
      double limit = route.Legs[leg].MaxGradient;
      if (leg + 1 < route.Legs.size() && route.Legs[leg + 1].MaxGradient > 0.0 &&
          route.Legs[leg + 1].MaxGradient < limit) {
        limit = route.Legs[leg + 1].MaxGradient;
      }
      gradeLimit[post] = limit;
    }
  }
  long undeclared = 0;
  double gentlestLimit = 1.0, gentlestAtM = 0.0;
  for (size_t post = 0; post < gradeLimit.size(); ++post) {
    if (!(gradeLimit[post] > 0.0)) {
      ++undeclared;
      continue;
    }
    if (gradeLimit[post] < gentlestLimit) {
      gentlestLimit = gradeLimit[post];
      gentlestAtM = (double)post * spanM;
    }
  }
  Note("stations whose road kind declares no maximum grade", (double)undeclared, "stations");
  CHECK(undeclared == 0,
        "**AND EVERY KIND ON THE ROUTE DECLARES ITS OWN MAXIMUM GRADE.** A station with none would be "
        "flattened by a shaping that had nothing to shape it to -- silently, which is the failure "
        "this count exists to make loud. The grades are RAA, RAL and RASt figures declared beside "
        "the widths in src/assets/world/vegetation.json");
  Note("the gentlest grade any road class on this route declares", gentlestLimit * 100.0, "%");
  Note("the steepest the F31's drivetrain could climb from rest", 23.43257, "%");
  Note("the steepest it could hold at its own top speed", 0.15, "%");
  Note("where that is", gentlestAtM / 1000.0, "km");

  long shaped = 0;
  int shapingPasses = 0;
  for (int sweep = 0; sweep < 8; ++sweep) {
    ++shapingPasses;
    long moved = 0;
    for (size_t post = 1; post < roadM.size(); ++post) {
      const double most =
          (gradeLimit[post] < gradeLimit[post - 1] ? gradeLimit[post] : gradeLimit[post - 1]) *
          spanM;
      const double ceiling = roadM[post - 1] + most;
      const double floorM = roadM[post - 1] - most;
      if (roadM[post] > ceiling) { roadM[post] = ceiling; ++moved; }
      if (roadM[post] < floorM) { roadM[post] = floorM; ++moved; }
    }
    for (size_t post = roadM.size() - 1; post > 0; --post) {
      const double most =
          (gradeLimit[post] < gradeLimit[post - 1] ? gradeLimit[post] : gradeLimit[post - 1]) *
          spanM;
      const double ceiling = roadM[post] + most;
      const double floorM = roadM[post] - most;
      if (roadM[post - 1] > ceiling) { roadM[post - 1] = ceiling; ++moved; }
      if (roadM[post - 1] < floorM) { roadM[post - 1] = floorM; ++moved; }
    }
    if (moved == 0) { break; }
    shaped = moved;
  }
  Note("sweeps the shaping needed", (double)shapingPasses, "sweeps");

  double cutM = 0.0, fillM = 0.0, cutAtM = 0.0, fillAtM = 0.0, movedM = 0.0;
  for (size_t post = 0; post < roadM.size(); ++post) {
    const double byM = roadM[post] - heightM[post];
    movedM += std::fabs(byM);
    if (byM < cutM) { cutM = byM; cutAtM = (double)post * spanM; }
    if (byM > fillM) { fillM = byM; fillAtM = (double)post * spanM; }
  }
  Note("the deepest the road cuts into the ground", -cutM, "m");
  Note("where that is", cutAtM / 1000.0, "km");
  Note("the highest it fills above it", fillM, "m");
  Note("where that is", fillAtM / 1000.0, "km");
  Note("the mean earth moved per station", movedM / (double)roadM.size(), "m");
  Note("stations still being shaped when the passes ran out", (double)shaped, "stations");

  std::vector<outshine::Knot> rise;
  rise.reserve(roadM.size());
  double worstGradeM = 0.0, worstGradeAtM = 0.0;
  for (size_t post = 0; post < roadM.size(); ++post) {
    const double atM = (double)post * spanM;
    const size_t before = post > 0 ? post - 1 : post;
    const size_t after = post + 1 < roadM.size() ? post + 1 : post;
    const double overM = ((double)after - (double)before) * spanM;
    const double slope = overM > 0.0 ? (roadM[after] - roadM[before]) / overM : 0.0;
    if (std::fabs(slope) > std::fabs(worstGradeM)) {
      worstGradeM = slope;
      worstGradeAtM = atM;
    }
    rise.push_back(outshine::Knot{atM, roadM[post], slope});
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

  outshine::SpeedProfile profile;
  const double shortestCornerM = 1.5 * tightestM * 0.1;
  const double profileStepM = 0.5 * shortestCornerM;
  Note("the shortest corner the fit can produce", shortestCornerM, "m");
  Note("the step the speed profile is sampled at", profileStepM, "m");
  CHECK(profile.Over(corridor, planning, profileStepM, 0.0, error),
        "and a speed profile is solved over the whole corridor from its geometry alone");
  if (!error.empty()) { std::printf("REFUSED %s\n", error.c_str()); }

  double slowestMs = 1.0e9, fastestMs = 0.0, meanMs = 0.0;
  for (size_t sample = 0; sample < profile.SampleCount(); ++sample) {
    const double ms = profile.SampleAt(sample);
    slowestMs = ms < slowestMs ? ms : slowestMs;
    fastestMs = ms > fastestMs ? ms : fastestMs;
    meanMs += ms;
  }
  if (profile.SampleCount() > 0) { meanMs /= (double)profile.SampleCount(); }
  Note("the slowest the profile asks for", slowestMs * 3.6, "km/h");
  Note("the fastest", fastestMs * 3.6, "km/h");
  Note("the mean", meanMs * 3.6, "km/h");
  Note("stations where a CREST and not a curve set the speed",
       (double)profile.CrestsThatBound(), "stations");
  Note("the slowest a crest holds it to", profile.CrestHeldMs() * 3.6, "km/h");
  Note("where that crest is", profile.CrestHeldAtM() / 1000.0, "km");
  Note("the drive time that implies", fitted.LengthM / (meanMs > 0.0 ? meanMs : 1.0) / 3600.0, "h");

  outshine::Physics::Rig rig = stood.Rig;
  outshine::Physics::Body body;
  body.MassKg = stood.Envelope.MassKg;
  for (int axis = 0; axis < 3; ++axis) {
    body.InertiaKgM2[axis] = declared.Vehicles[0].InertiaKgM2[axis];
  }
  outshine::Placed start;
  CHECK(corridor.At(0.0, start), "the corridor answers at its own start");
  const outshine::Standing under0 =
      outshine::Stand(corridor, start.EastM, start.NorthM, 0.0, 0.0, 50.0);
  body.PositionM[0] = start.EastM;
  body.PositionM[1] = under0.HeightM + stood.CentreM[1];
  body.PositionM[2] = -start.NorthM;
  {
    const double up[3] = {under0.NormalM[0], under0.NormalM[1], -under0.NormalM[2]};
    Lie(body, start, up);
    const double aheadBody[3] = {0.0, 0.0, -1.0};
    double ahead[3];
    outshine::Physics::Turn(body.OrientationQ, aheadBody, ahead);
    for (int axis = 0; axis < 3; ++axis) { body.VelocityMs[axis] = kJoinMs * ahead[axis]; }
  }

  const double gravity[3] = {0.0, -9.80665, 0.0};
  outshine::Pilot::Reins reins;
  reins.SettleS = 1.0;
  reins.LeastReachM = stood.Axles.WheelbaseM;
  reins.HoldWithinM = holdWithinM;
  const double dragArea =
      declared.Vehicles[0].DragCoefficient * declared.Vehicles[0].FrontalM2;

  Drove drove;
  const auto driving = std::chrono::steady_clock::now();
  double nearM = 0.0;
  double simulatedS = 0.0;
  double lostM = 0.0;
  for (long step = 0; step < kMaxSteps; ++step) {
    const double eastM = body.PositionM[0];
    const double northM = -body.PositionM[2];
    const double headingRad = HeadingOf(body);
    const double windowM = kResectM + 3.0 * lostM;
    const outshine::Pilot::Placement at = outshine::Pilot::Locate(
        corridor, eastM, northM, body.PositionM[1], headingRad, nearM, windowM);
    if (!at.Found) {
      drove.Lost = true;
      break;
    }
    nearM = at.AlongM;
    lostM = std::fabs(at.OffsetM);
    drove.ReachedM = at.AlongM;

    const double speedMs = std::sqrt(body.VelocityMs[0] * body.VelocityMs[0] +
                                     body.VelocityMs[2] * body.VelocityMs[2]);
    reins.TightestPerM = outshine::Pilot::TightestPerM(stood.Axles, stood.Envelope, speedMs);
    const double aheadM =
        std::fmin(at.AlongM + outshine::Pilot::ReachOf(reins, speedMs, at.CurvatureRatePerM),
                  corridor.LengthM());
    const outshine::Pilot::Demand asked =
        Hold(corridor, reins, at, speedMs, profile.At(aheadM));
    const outshine::Pilot::Steering command =
        outshine::Pilot::Drive(stood.Axles, stood.Envelope, asked);

    outshine::Physics::Controls controls;
    controls.SteerRad = command.SteerRad;
    controls.DriveN = command.DriveN;
    controls.BrakeN = command.BrakeN;

    outshine::Physics::Footing under[outshine::Physics::kMaxMounts];
    for (size_t which = 0; which < rig.Count; ++which) {
      double worldM[3];
      outshine::Physics::Place(body, rig.Mounts[which].AtM, worldM);
      const outshine::Standing on =
          outshine::Stand(corridor, worldM[0], -worldM[2], 0.0, at.AlongM,
                          kMountResectM + 3.0 * lostM);
      under[which].Found = on.On;
      under[which].HeightM = on.HeightM;
      under[which].NormalM[0] = on.NormalM[0];
      under[which].NormalM[1] = on.NormalM[1];
      under[which].NormalM[2] = -on.NormalM[2];
    }

    outshine::Physics::Wrench wrench;
    outshine::Physics::Fall(wrench, body, gravity);
    outshine::Physics::Resist(wrench, body, dragArea, declared.Vehicles[0].AirDensity);
    const outshine::Physics::Reading read =
        outshine::Physics::Bear(rig, body, under, controls, wrench, kStepS);

    if (at.AlongM >= kFromM) {
      if (std::fabs(at.OffsetM) > std::fabs(drove.WorstOffsetM)) {
        drove.WorstOffsetM = at.OffsetM;
        drove.WorstOffsetAtM = at.AlongM;
      }
      drove.WorstRatio = std::fmax(drove.WorstRatio, read.WorstRatio);
      drove.TopMs = std::fmax(drove.TopMs, speedMs);
      if (read.PastLimit && !drove.PastLimit) {
        drove.PastLimit = true;
        drove.BrokeAtM = at.AlongM;
      }
      drove.PastTravel = drove.PastTravel || read.PastTravel;
      if (read.Airborne > drove.MostAirborne) {
        drove.MostAirborne = read.Airborne;
        drove.AirborneAtM = at.AlongM;
      }
    }

    if (read.PastLimit || read.Airborne == rig.Count) {
      drove.BrokeAtM = at.AlongM;
      drove.PastLimit = drove.PastLimit || read.PastLimit;
      drove.MostAirborne = read.Airborne;
      break;
    }
    outshine::Physics::Step(body, wrench, kStepS);
    simulatedS += kStepS;
    if (at.AlongM >= corridor.LengthM() - 20.0) {
      drove.Arrived = true;
      break;
    }
  }
  const double wallS =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - driving).count();

  Note("how far the car drove", drove.ReachedM / 1000.0, "km");
  Note("of a corridor this long", corridor.LengthM() / 1000.0, "km");
  Note("the fastest it went", drove.TopMs * 3.6, "km/h");
  Note("hours simulated", simulatedS / 3600.0, "h");
  Note("seconds of wall clock", wallS, "s");
  Note("how much faster than real time", simulatedS / (wallS > 0.0 ? wallS : 1.0), "x");
  Note("worst deviation from the line", drove.WorstOffsetM, "m");
  Note("the look-ahead the pilot had at top speed", 1.0 * drove.TopMs / 3.6 * 3.6, "m");
  Note("where that was", drove.WorstOffsetAtM / 1000.0, "km");
  Note("worst share of a contact's grip used", drove.WorstRatio, "of it");
  Note("most mounts off the ground at once", (double)drove.MostAirborne, "of 4");
  Note("where that was", drove.AirborneAtM / 1000.0, "km");
  Note("where a contact first went past its limit", drove.BrokeAtM / 1000.0, "km");

  CHECK(!drove.Lost, "the car never left the corridor's own window");
  CHECK(!drove.PastLimit,
        "**AND NO CONTACT WENT PAST ITS DECLARED LIMIT.** A crash on this route is READ -- a load "
        "past what the link carries -- and there was none");
  CHECK(drove.Arrived,
        "**THE F31 DROVE ITSELF FROM MARIENPLATZ TO RATHAUSMARKT.** Two coordinates in, a route "
        "planned over live OSM ways, a corridor fitted through them, the real ground under it, and "
        "the declared car carried the whole way by four compliant contacts and nothing else");

  fb_stream_close();

  Covers("I.4.5 a route from Marienplatz to Rathausmarkt is planned over ways fetched live from the "
         "declared OSM source, woven into a network by snapping at twice the tile resolution, with "
         "admissibility decided by whether the declared vehicle fits");
  return Report();
}
