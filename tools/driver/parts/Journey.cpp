#include <cstdarg>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "Journey.h"

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

constexpr double kF31WidthM = 1.811;

constexpr double kPatienceS = 900.0;
constexpr double kResectM = 4.0;
constexpr double kJoinMs = 20.0;
constexpr double kFromM = 50.0;
constexpr double kMountResectM = 8.0;

struct Unused_Drove {
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
  double LeftTheRoadAtM = 0.0;
  double LeftByM = 0.0;
  double LeftAtMs = 0.0;
  double LeftPlannedMs = 0.0;
  double LeftCurvature = 0.0;
  double LeftRate = 0.0;
  double LeftLaneM = 0.0;
  double LeftEdgeM = 0.0;
  double LeftAsideM = 0.0;
  double LeftAcrossM = 0.0;
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


namespace outshine::Driver {

struct Journey::State {
  std::unique_ptr<outshine::Data::ContentStore> Store;
  std::unique_ptr<outshine::Data::SourceSet> Sources;
  std::unique_ptr<outshine::Host::CurlTransport> Wire;
  outshine::Scenario Declared;
  outshine::Clients::Rigged Stood;
  outshine::ReferenceLine Corridor;
  outshine::Fitted Fitted;
  outshine::SpeedProfile Profile;
  outshine::Physics::Rig Rig;
  outshine::Physics::Body Body;
  std::vector<double> RoadM, HalfWidthM, LaneHalfM, AsideM, FineAside, FineEdge;
  double FineM = 2.0;
  double SpanM = 0.0;
  double NarrowestLaneM = 0.0;
  double BudgetM = 0.0;
  double HoldWithinM = 0.0;
  double AsideRatePerM = 0.0;
  double HeldAsideM = 0.0;
  bool HaveAside = false;
  double NearM = 0.0;
  double LostM = 0.0;
  double SimulatedS = 0.0;
  bool Opened = false;
  bool Ready = false;
  Ridden Tally;
};

Journey::Journey() : S_(std::make_unique<State>()) {}
Journey::~Journey() { Close(); }

void Journey::Close(void) {
  if (S_ && S_->Opened) {
    fb_stream_close();
    S_->Opened = false;
  }
}

const outshine::Physics::Body &Journey::Carried(void) const { return S_->Body; }
const outshine::ReferenceLine &Journey::Corridor(void) const { return S_->Corridor; }
const outshine::Scenario &Journey::Declared(void) const { return S_->Declared; }
double Journey::LengthM(void) const { return S_->Corridor.LengthM(); }

namespace {

std::string Line(const char *shape, const std::string &one) {
  std::string out = shape;
  const size_t at = out.find("%s");
  if (at != std::string::npos) { out.replace(at, 2, one); }
  return out;
}

std::string Line(const char *shape, const char *one) { return Line(shape, std::string(one)); }

std::string Line(const char *shape, ...) {
  char held[512];
  va_list rest;
  va_start(rest, shape);
  std::vsnprintf(held, sizeof(held), shape, rest);
  va_end(rest);
  return std::string(held);
}

} // namespace

bool Journey::Lay(const Between &between, const char *scenarioPath, int zoom, Sink &say) {
  const double kMunichLat = between.FromLatDeg;
  const double kMunichLon = between.FromLonDeg;
  const double kHamburgLat = between.ToLatDeg;
  const double kHamburgLon = between.ToLonDeg;
  const int kZoom = zoom;

  auto &corridor = S_->Corridor;
  auto &profile = S_->Profile;
  auto &declared = S_->Declared;
  auto &stood = S_->Stood;
  auto &fitted = S_->Fitted;
  auto &roadM = S_->RoadM;
  auto &halfWidthM = S_->HalfWidthM;
  auto &laneHalfM = S_->LaneHalfM;
  auto &asideM = S_->AsideM;
  auto &fineAside = S_->FineAside;
  auto &fineEdge = S_->FineEdge;
  const double fineM = S_->FineM;
  auto &spanM = S_->SpanM;
  auto &narrowestLaneM = S_->NarrowestLaneM;
  auto &budgetM = S_->BudgetM;
  auto &holdWithinM = S_->HoldWithinM;


  const double straightM = ApartM(kMunichLat, kMunichLon, kHamburgLat, kHamburgLon);
  const double middleLat = 0.5 * (kMunichLat + kHamburgLat);
  const double middleLon = 0.5 * (kMunichLon + kHamburgLon);
  say.Number("Marienplatz to Rathausmarkt as the crow flies", straightM / 1000.0, "km");
  say.Number("the zoom the ways are read at", (double)kZoom, "");
  const double tileGroundM =
      40075017.0 * std::cos(middleLat * 3.14159265358979 / 180.0) / (double)(1L << kZoom);
  const int kCorridorRing = 2;
  const long steps = (long)std::ceil(straightM / tileGroundM) + 1;
  const long square = (long)(2 * std::ceil(0.5 * straightM / tileGroundM) + 3);
  say.Number("a tile's ground size at this zoom and latitude", tileGroundM / 1000.0, "km");
  say.Number("stations along the line the corridor is fetched at", (double)steps, "stations");
  say.Number("the ring fetched around each", (double)(2 * kCorridorRing + 1), "tiles across");
  say.Number("what a square covering both cities would have cost", (double)(square * square), "tiles");

  outshine::Data::ContentStore::Config keeping;
  keeping.Directory = "/tmp/outshine-drive-cache";
  outshine::Data::ContentStore store(keeping);
  outshine::Data::SourceSet sources(store);
  say.Claim(outshine::Data::RegisterDeclared(sources, {"src/assets/sky", true}) ==
            outshine::Data::Registered::Complete,
        "the declared upstream sources register, ranked and without a clash");
  say.Number("sources registered", (double)sources.Count(), "sources");

  outshine::Host::CurlTransport::Config wiring;
  wiring.Threads = 8;
  outshine::Host::CurlTransport wire(wiring);
  FbGroundSurface surface;
  surface.Z = 12;
  surface.Grid = 64;
  const int opened = fb_stream_open(sources, wire, middleLat, middleLon, surface);
  say.Claim(opened == 1,
        "the streamer opens over the declared sources -- and it is a GLOBAL, which is what "
        "OsmField reaches for. CLAUDE.md gives neither a global nor a singleton a place to live, "
        "and board:1527 is that finding");
  if (opened != 1) { return false; }

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
  say.Claim(!ranOut, "the corridor is fetched inside the patience declared for it");
  const double fetchedS =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();

  say.Number("passes over the corridor", (double)passes, "passes");
  say.Number("tiles the corridor actually took", (double)field.Tiles().size(), "tiles");
  say.Number("tiles decoded", (double)built, "tiles");
  say.Number("tiles still pending", (double)field.PendingTiles(), "tiles");
  say.Number("layers the server did not send", (double)field.MissingLayers(), "layers");
  say.Number("tiles that would not decode", (double)field.BadTiles(), "tiles");
  say.Number("features read", (double)field.Features().size(), "features");
  say.Number("points in them", (double)(field.Points().size() / 2), "points");
  say.Number("seconds spent fetching and decoding", fetchedS, "s");

  say.Claim(built > 0 && field.Features().size() > 0,
        "**REAL OSM WAYS ARRIVE OVER THE WIRE AND DECODE.** No fixture, no committed extract: the "
        "declared upstream source is asked for the tiles between Munich and Hamburg and answers "
        "with vector geometry");
  if (field.Features().empty()) { return false; }

  outshine::World::GroundMaterials materials;
  say.Claim(materials.Load("src/assets/world/ground-materials.json"),
        "the declared ground materials load, because a width table names them");
  VegetationTemplates widths;
  std::string error;
  say.Claim(widths.Load("src/assets/world/vegetation.json", materials),
        "the declared widths load, with their RAA, RAL and RASt origins");

  const double quantumM = 40075017.0 / ((double)(1L << kZoom) * 4096.0);
  Network roads(1.05 * quantumM);
  say.Number("the tile's own coordinate quantisation", quantumM, "m");
  const Reaped reaped = Reap(field, widths, kF31WidthM, roads);
  say.Number("the snapping distance just above it", roads.SnapM(), "m");
  say.Number("ways a car can fit down", (double)reaped.Ways, "ways");
  say.Number("points in them", (double)reaped.Points, "points");
  say.Number("ways too narrow for it", (double)reaped.TooNarrow, "ways");
  say.Number("the widest way it refused", reaped.WidestRefusedM, "m");
  say.Number("the narrowest it took", reaped.NarrowestTakenM, "m");
  say.Number("ways whose kind carries no declared width", (double)reaped.Unclassed, "ways");

  say.Number("ways that are not a carriageway at all", (double)reaped.NotACarriageway, "ways");
  if (!reaped.NotCarriageways.empty()) {
    say.Say(Line("NOT CARRIAGEWAYS %s", reaped.NotCarriageways.c_str()));
  }
  say.Claim(reaped.NotACarriageway > 0,
        "**AND A RAILWAY IS NOT A ROAD, WHICH WIDTH ALONE NEVER SAID.** The streets layer carries "
        "rail, tram, subway, monorail, funicular, narrow_gauge and light_rail, and a rail ballast "
        "crown is 3.8 m -- wider than the car, so the width test passed them and the router put the "
        "F31 on the tracks. What a CARRIAGEWAY has is LANES; a railway declares none, and that is "
        "the test");
  say.Number("ways whose kind declares no maximum grade", (double)reaped.Ungraded, "ways");
  say.Number("ways whose kind declares no lane count", (double)reaped.Unlaned, "ways");
  if (!reaped.WithoutGrade.empty()) {
    say.Say(Line("UNGRADED KINDS %s", reaped.WithoutGrade.c_str()));
  }
  if (!reaped.WithoutLanes.empty()) {
    say.Say(Line("UNLANED KINDS %s", reaped.WithoutLanes.c_str()));
  }
  say.Claim(reaped.Ways > 0, "and roads a 1.811 m car can fit down are harvested from them");
  say.Claim(reaped.WidestRefusedM < kF31WidthM || reaped.TooNarrow == 0,
        "**ADMISSIBILITY IS THE VEHICLE'S WIDTH AND NOT A LIST OF TAGS.** Every way this refused is "
        "narrower than the car; every way it took is wider. Nobody wrote down which highway kinds a "
        "car may use, and traffic law stays unmodelled -- what decides is whether it fits");

  say.Claim(roads.Weave(error), "the ways weave into a network");
  if (!error.empty()) { say.Say(Line("REFUSED %s", error.c_str())); }
  say.Number("nodes after snapping", (double)roads.NodeCount(), "nodes");
  say.Number("junctions among them", (double)roads.JunctionCount(), "nodes");
  say.Number("edges", (double)roads.EdgeCount(), "edges");

  size_t atMunich = 0, atHamburg = 0;
  double munichAwayM = 0.0, hamburgAwayM = 0.0;
  say.Claim(roads.Nearest(Waypoint{kMunichLat, kMunichLon}, atMunich, munichAwayM) &&
            roads.Nearest(Waypoint{kHamburgLat, kHamburgLon}, atHamburg, hamburgAwayM),
        "both city centres resolve to a node of the network");
  say.Number("how far Marienplatz is from the nearest road node", munichAwayM, "m");
  say.Number("how far Rathausmarkt is from the nearest road node", hamburgAwayM, "m");
  say.Number("how far each walk is as a share of the drive",
       (munichAwayM + hamburgAwayM) / straightM, "of it");
  say.Claim(munichAwayM + hamburgAwayM < 0.001 * straightM,
        "**AND THE WALK AT EACH END IS NEGLIGIBLE AGAINST THE DRIVE.** Both squares are pedestrian "
        "zones, so the nearest CARRIAGEWAY is a few hundred metres away and the car parks at the "
        "edge -- 267 m at Marienplatz and 221 m at Rathausmarkt, together under a thousandth of the "
        "612 km between them. The bound is the route's own length and not a number somebody picked: "
        "a node 200 km away would give a route shorter than the great circle and still look like "
        "one");

  const double tightestM = 2.810 / std::tan(0.522804742);
  say.Number("the tightest circle the F31 can turn", tightestM, "m");
  const Route route =
      roads.Plan(Waypoint{kMunichLat, kMunichLon}, Waypoint{kHamburgLat, kHamburgLon}, tightestM,
                 quantumM);
  say.Number("turns the search refused as too sharp for the car", (double)route.TurnsRefused, "turns");
  if (!route.Found) { say.Say(Line("REFUSED %s", route.Error.c_str())); }
  say.Number("nodes the search settled", (double)route.Reached, "nodes");
  say.Claim(route.Found,
        "**AND A ROUTE IS FOUND FROM MARIENPLATZ TO RATHAUSMARKT OVER OSM'S OWN WAYS.** Two "
        "coordinates in, a chain of real roads out, nothing stored and nothing hand-placed");
  if (!route.Found) { return false; }

  say.Number("how far the route runs", route.LengthM / 1000.0, "km");
  say.Number("how far the crow flies", route.StraightM / 1000.0, "km");
  say.Number("the detour that is", route.LengthM / route.StraightM, "x");
  say.Claim(route.LengthM > route.StraightM,
        "**A ROAD CANNOT BE SHORTER THAN THE GREAT CIRCLE.** If it is, the route did not run between "
        "the two places asked for, or the network welded roads that do not meet -- and both are "
        "findings rather than a route");
  say.Number("legs in it", (double)route.Legs.size(), "legs");

  say.Claim(route.LengthM / 1000.0 > 700.0 && route.LengthM / 1000.0 < 900.0,
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
  say.Number("metres per degree of latitude in the local frame", perLatM, "m");
  say.Number("metres per degree of longitude there", perLonM, "m");

  const std::vector<double> keptM = outshine::Simplify(eastNorthM, quantumM);
  say.Number("vertices the route offered before simplifying", (double)(eastNorthM.size() / 2),
       "vertices");
  say.Number("vertices left after removing what the data cannot resolve",
       (double)(keptM.size() / 2), "vertices");
  say.Number("the share removed", 1.0 - (double)keptM.size() / (double)eastNorthM.size(), "of them");

    fitted = Fit(keptM, quantumM, tightestM, corridor);
  if (!fitted.Laid) { say.Say(Line("REFUSED %s", fitted.Error.c_str())); }
  say.Number("vertices the route offered", (double)fitted.Vertices, "vertices");
  say.Number("corners the fit needed", (double)fitted.Corners, "corners");
  say.Number("straights between them", (double)fitted.Straights, "straights");
  say.Number("the corridor it laid", fitted.LengthM / 1000.0, "km");
  say.Number("the polyline it came from", route.LengthM / 1000.0, "km");
  say.Number("the tightest radius on it", fitted.TightestRadiusM, "m");
  say.Number("the sharpest turn it carried", fitted.SharpestTurnRad * 180.0 / 3.14159265358979, "deg");
  say.Number("at which vertex", fitted.SharpestTurnAtM, "");
  say.Number("turns past a right angle", (double)fitted.TurnsPastRightAngle, "of 2480");
  say.Number("turns past 135 degrees", (double)fitted.TurnsPastHalfCircle, "of 2480");
  say.Number("vertices too sharp for the car to drive at all", (double)fitted.Undrivable, "vertices");
  say.Number("how far it leaves a vertex at worst", fitted.WorstOffsetM, "m");
  say.Number("corners the fit had to correct by measuring them", (double)fitted.Corrected, "corners");
  say.Number("passes it needed", (double)fitted.Passes, "passes");
  say.Number("how far the laid line drifts from the polyline beyond any corner's own doing",
       fitted.DriftM, "m");
  say.Number("per corner that is", fitted.DriftPerCornerM * 1000.0, "mm");
  say.Number("the worst vertex", fitted.WorstVertex, "");
  say.Number("its incoming leg", fitted.WorstLegInM, "m");
  say.Number("its outgoing leg", fitted.WorstLegOutM, "m");
  say.Number("the turn there", fitted.WorstTurnRad * 180.0 / 3.14159265358979, "deg");
  say.Number("the radius it settled on", fitted.WorstRadiusM, "m");
  say.Number("the station the fit expected it at", fitted.WorstExpectedM, "m");
  say.Number("the station the resection found", fitted.WorstStationM, "m");
  say.Number("corners the data cannot support at any drivable radius", (double)fitted.Strained,
       "corners");
  say.Number("how far the worst of those leaves its vertex", fitted.StrainedWorstM, "m");
  say.Number("where that happens", fitted.WorstOffsetAtM / 1000.0, "km");
  say.Number("how far it is allowed to", quantumM, "m");

  if (!fitted.Laid) { say.Say(Line("REFUSED %s", fitted.Error.c_str())); }
  if (!fitted.Laid && fitted.Undrivable > 0) {
    const size_t at = (size_t)fitted.UndrivableAtM;
    for (size_t which = at > 1 ? at - 1 : 0; which <= at + 1 && which < route.Legs.size(); ++which) {
      say.Say(Line("AT %zu  %.7f %.7f", which, route.Legs[which].At.LatDeg,
                  route.Legs[which].At.LonDeg));
    }
    if (at >= 1 && at + 1 < route.Legs.size()) {
      say.Say(Line("LEGS in %.2f m  out %.2f m", ApartM(route.Legs[at - 1].At.LatDeg, route.Legs[at - 1].At.LonDeg,
                         route.Legs[at].At.LatDeg, route.Legs[at].At.LonDeg),
                  ApartM(route.Legs[at].At.LatDeg, route.Legs[at].At.LonDeg,
                         route.Legs[at + 1].At.LatDeg, route.Legs[at + 1].At.LonDeg)));
    }
  }
  say.Claim(fitted.Laid,
        "**AND THE ROUTE BECOMES A CORRIDOR WITH NO CURVATURE LEAP ANYWHERE ON IT.** 752 km of OSM "
        "polyline, every interior vertex carrying spiral-arc-spiral, laid by a ReferenceLine that "
        "REFUSES a leap -- so a step in the lateral force has no spelling on this road");
  if (!fitted.Laid) { return false; }
  say.Claim(fitted.DriftM < 0.05 * quantumM * (double)fitted.Corners,
        "**AND WHAT IS LEFT IS DRIFT, WHICH NO CORNER CAN CORRECT.** The line is walked corner by "
        "corner and each spiral is integrated by 8-node Gauss-Legendre; the residual accumulates "
        "laterally over 2300 corners. It is reported as its own term, in millimetres per corner, "
        "rather than being chased by shrinking corners that were never at fault -- which is what "
        "pinned 1284 of them at the vehicle's tightest radius. board:1528");
  say.Claim(fitted.Strained * 200 < fitted.Corners,
        "**AND WHERE THE DATA CANNOT SUPPORT A ROAD AT ANY RADIUS THE CAR CAN TURN, THAT CORNER IS "
        "COUNTED AND NOT HIDDEN.** Those are the corners whose vertices the line must leave by more "
        "than the tile's own quantisation to stay drivable at all -- a classified finding with a "
        "count, not a fit that quietly bent further. Fewer than one in two hundred here");
  say.Number("how much longer the corridor is than the polyline",
       fitted.LengthM / route.LengthM - 1.0, "of it");
  say.Claim(fitted.LengthM < 1.05 * route.LengthM,
        "and within a few per cent of the polyline it was fitted through -- a cut corner is shorter "
        "than the corner, and the 1.6 % it runs long is the first-order construction on the tail of "
        "sharp turns, board:1528");
  say.Number("the speed the tightest radius allows at 0.95 g",
       std::sqrt(0.95 * 9.80665 * fitted.TightestRadiusM) * 3.6, "km/h");

  std::FILE *const declaration = std::fopen("tools/driver/f31.scenario", "rb");
  say.Claim(declaration != nullptr, "the F31's own declaration is there to read");
  std::vector<char> text;
  if (declaration != nullptr) {
    int one = 0;
    while ((one = std::fgetc(declaration)) != EOF) { text.push_back((char)one); }
    std::fclose(declaration);
  }
  say.Claim(!text.empty() && outshine::ReadScenario(text.data(), text.size(), declared, error),
        "and it reads");
  say.Claim(declared.Vehicles.size() == 1, "declaring one vehicle");
  if (declared.Vehicles.empty()) { return false; }

  stood = outshine::Clients::Stand(declared.Vehicles[0]);
  if (!stood.Stood) { say.Say(Line("REFUSED %s", stood.Error.c_str())); }
  say.Claim(stood.Stood, "**AND THE DECLARED F31 STANDS UP AS A RIG.** Every number the drive uses comes "
                     "from the file, not from a constant beside it");
  if (!stood.Stood) { return false; }


  const double postM = fb_stream_ground_post_m(middleLat);
  const long posts = (long)std::ceil(fitted.LengthM / postM);
  say.Number("the elevation source's own post spacing here", postM, "m");
  say.Number("stations the corridor is sampled at", (double)(posts + 1), "stations");

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
  say.Number("stations the elevation source answered", (double)resolved, "stations");
  say.Number("stations it said were a hole", (double)holes, "stations");
  say.Number("times a sample had to wait for a tile", (double)waited, "waits");
  say.Number("seconds spent sampling the ground", sampledS, "s");
  say.Number("the lowest the corridor runs", lowestM, "m");
  say.Number("the highest", highestM, "m");
  say.Number("Munich's own elevation", heightM.empty() ? 0.0 : heightM.front(), "m");
  say.Number("Hamburg's", heightM.empty() ? 0.0 : heightM.back(), "m");

  say.Claim(resolved > 0, "**THE ELEVATION SOURCE ANSWERS ALONG THE WHOLE CORRIDOR.** Real height data, "
                      "streamed for the same route the ways came from");
  say.Claim(holes == 0, "with no hole in it -- a hole is a named refusal and there is none here");
  say.Claim(std::fabs(heightM.front() - 523.0) < 40.0 && std::fabs(heightM.back() - 14.0) < 40.0,
        "**AND THE TWO ENDS ARE WHERE THE CITIES ARE.** Munich stands at about 520 m and Hamburg at "
        "about 10; the source says 523.15 and 14.14, which is the check that this is the real world "
        "and not a plausible surface");

  outshine::SpeedProfile inPlan;
  say.Claim(inPlan.Over(corridor, stood.Envelope, postM, 0.0, error),
        "the plan view alone gives a speed at every station, before the ground is consulted");

  spanM = fitted.LengthM / (double)posts;
  roadM = heightM;
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
  halfWidthM.assign(roadM.size(), 0.0);
  {
    size_t leg = 0;
    for (size_t post = 0; post < halfWidthM.size(); ++post) {
      const double atM = (double)post * spanM * route.LengthM / fitted.LengthM;
      while (leg + 1 < route.Legs.size() && route.Legs[leg + 1].AlongM < atM) { ++leg; }
      double half = route.Legs[leg].HalfWidthM;
      if (leg + 1 < route.Legs.size() && route.Legs[leg + 1].HalfWidthM > 0.0 &&
          route.Legs[leg + 1].HalfWidthM < half) {
        half = route.Legs[leg + 1].HalfWidthM;
      }
      halfWidthM[post] = half;
    }
  }
  laneHalfM.assign(roadM.size(), 0.0);
  asideM.assign(roadM.size(), 0.0);
  long laneless = 0;
  {
    size_t leg = 0;
    for (size_t post = 0; post < laneHalfM.size(); ++post) {
      const double atM = (double)post * spanM * route.LengthM / fitted.LengthM;
      while (leg + 1 < route.Legs.size() && route.Legs[leg + 1].AlongM < atM) { ++leg; }
      const int lanes = route.Legs[leg].Lanes;
      if (lanes <= 0) {
        ++laneless;
        laneHalfM[post] = halfWidthM[post];
        continue;
      }
      const double laneM = 2.0 * halfWidthM[post] / (double)lanes;
      laneHalfM[post] = 0.5 * laneM;
      asideM[post] = -0.5 * (double)(lanes - 1) * laneM;
    }
  }
  say.Number("stations whose road kind declares no lane count", (double)laneless, "stations");
  say.Claim(laneless == 0,
        "**AND EVERY KIND ON THE ROUTE DECLARES HOW MANY LANES IT CARRIES.** The lane count comes from "
        "the same cross-sections the widths do -- RAA RQ 28 is two 3.75 m running lanes and a 2.5 m "
        "shoulder per one-way carriageway -- so a car's lane is the width over the count and not the "
        "whole road");

  double steppedM = 0.0, steppedAtM = 0.0;
  for (size_t post = 1; post < asideM.size(); ++post) {
    const double step = std::fabs(asideM[post] - asideM[post - 1]);
    if (step > steppedM) {
      steppedM = step;
      steppedAtM = (double)post * spanM;
    }
  }
  say.Number("the largest step the lane centre takes where the road changes width", steppedM, "m");
  say.Number("where that is", steppedAtM / 1000.0, "km");

  double narrowestLaneHereM = 1.0e9;
  for (const double half : laneHalfM) {
    narrowestLaneHereM = 2.0 * half < narrowestLaneHereM ? 2.0 * half : narrowestLaneHereM;
  }
  budgetM = 0.5 * narrowestLaneHereM - 0.5 * kF31WidthM;

  fineAside.assign((size_t)(fitted.LengthM / fineM) + 2, 0.0);
  fineEdge.assign(fineAside.size(), 0.0);
  for (size_t fine = 0; fine < fineAside.size(); ++fine) {
    const size_t post = (size_t)((double)fine * fineM / spanM);
    const size_t band = post < asideM.size() ? post : asideM.size() - 1;
    fineAside[fine] = asideM[band];
    fineEdge[fine] = halfWidthM[band];
  }
  {
    const double reachM = 1.0 * 232.722657 / 3.6;
    const double mostPerM = budgetM / reachM;
    say.Number("the fastest the lane centre may move sideways", mostPerM * 1000.0, "mm per metre");
    say.Number("so a 1.125 m shift is taken over", 1.125 / mostPerM, "m of road");
    const double most = mostPerM * fineM;

    std::vector<double> roomM(fineAside.size(), 0.0);
    long insideTight = 0;
    double worstDrivenM = 1.0e9;
    for (size_t fine = 0; fine < roomM.size(); ++fine) {
      double room = fineEdge[fine] - 0.5 * kF31WidthM - S_->BudgetM;
      outshine::Placed on;
      if (corridor.At((double)fine * fineM, on) && on.CurvaturePerM != 0.0) {
        const double radiusM = 1.0 / std::fabs(on.CurvaturePerM);
        const double inside = radiusM - tightestM;
        worstDrivenM = radiusM < worstDrivenM ? radiusM : worstDrivenM;
        if (inside < room) {
          room = inside;
          ++insideTight;
        }
      }
      roomM[fine] = room > 0.0 ? room : 0.0;
    }
    say.Number("the tightest the corridor itself turns", worstDrivenM, "m");
    say.Number("stations where the corner is too tight to hold two lanes apart",
         (double)insideTight, "stations");
    double leadM = 0.0;
    for (size_t fine = roomM.size() - 1; fine > 0; --fine) {
      const double reachable = roomM[fine] + most;
      if (roomM[fine - 1] > reachable) {
        leadM = std::fmax(leadM, roomM[fine - 1] - reachable);
        roomM[fine - 1] = reachable;
      }
    }
    for (size_t fine = 1; fine < roomM.size(); ++fine) {
      const double reachable = roomM[fine - 1] + most;
      if (roomM[fine] > reachable) { roomM[fine] = reachable; }
    }
    say.Number("the tracking error the lane centre keeps clear of the edge", budgetM, "m");
    say.Number("the most a narrowing pulled the lane centre in ahead of itself", leadM, "m");
    long led = 0;
    for (size_t fine = 0; fine < fineAside.size(); ++fine) {
      if (fineAside[fine] > roomM[fine]) { fineAside[fine] = roomM[fine]; ++led; }
      if (fineAside[fine] < -roomM[fine]) { fineAside[fine] = -roomM[fine]; ++led; }
    }
    say.Number("stations where a narrowing ahead moved the car in early", (double)led, "stations");

    for (int sweep = 0; sweep < 400; ++sweep) {
      long moved = 0;
      for (size_t fine = 1; fine < fineAside.size(); ++fine) {
        if (fineAside[fine] > fineAside[fine - 1] + most) {
          fineAside[fine] = fineAside[fine - 1] + most;
          ++moved;
        }
        if (fineAside[fine] < fineAside[fine - 1] - most) {
          fineAside[fine] = fineAside[fine - 1] - most;
          ++moved;
        }
      }
      for (size_t fine = fineAside.size() - 1; fine > 0; --fine) {
        if (fineAside[fine - 1] > fineAside[fine] + most) {
          fineAside[fine - 1] = fineAside[fine] + most;
          ++moved;
        }
        if (fineAside[fine - 1] < fineAside[fine] - most) {
          fineAside[fine - 1] = fineAside[fine] - most;
          ++moved;
        }
      }
      if (moved == 0) { break; }
    }
    long clamped = 0;
    for (size_t fine = 0; fine < fineAside.size(); ++fine) {
      if (fineAside[fine] > roomM[fine]) { fineAside[fine] = roomM[fine]; ++clamped; }
      if (fineAside[fine] < -roomM[fine]) { fineAside[fine] = -roomM[fine]; ++clamped; }
    }
    say.Number("stations where the road edge overruled the taper", (double)clamped, "stations");

    double leftM = 0.0, worstOverM = 0.0;
    for (size_t fine = 1; fine < fineAside.size(); ++fine) {
      leftM = std::fmax(leftM, std::fabs(fineAside[fine] - fineAside[fine - 1]));
      const double outerM = std::fabs(fineAside[fine]) + 0.5 * kF31WidthM;
      if (outerM > fineEdge[fine]) {
        worstOverM = std::fmax(worstOverM, outerM - fineEdge[fine]);
      }
    }
    say.Number("the largest step left after tapering", leftM, "m");
    say.Number("the furthest the tapered lane centre pushes the car past a road edge", worstOverM, "m");
  }

  narrowestLaneM = 1.0e9;
  double widestLaneM = 0.0, mostAsideM = 0.0;
  for (size_t post = 0; post < laneHalfM.size(); ++post) {
    narrowestLaneM = 2.0 * laneHalfM[post] < narrowestLaneM ? 2.0 * laneHalfM[post] : narrowestLaneM;
    widestLaneM = 2.0 * laneHalfM[post] > widestLaneM ? 2.0 * laneHalfM[post] : widestLaneM;
    mostAsideM = std::fabs(asideM[post]) > mostAsideM ? std::fabs(asideM[post]) : mostAsideM;
  }
  say.Number("the narrowest LANE on the route", narrowestLaneM, "m");
  say.Number("the widest lane", widestLaneM, "m");
  say.Number("the furthest the car sits from the centreline", mostAsideM, "m");
  say.Number("what it leaves either side of itself in the narrowest lane",
       0.5 * narrowestLaneM - 0.5 * kF31WidthM, "m");

  double narrowestHalfM = 1.0e9, widestHalfM = 0.0;
  for (const double half : halfWidthM) {
    narrowestHalfM = half < narrowestHalfM ? half : narrowestHalfM;
    widestHalfM = half > widestHalfM ? half : widestHalfM;
  }
  say.Number("the narrowest the carriageway gets", 2.0 * narrowestHalfM, "m");
  say.Number("the widest", 2.0 * widestHalfM, "m");
  say.Number("the car's own width", kF31WidthM, "m");
  say.Claim(2.0 * narrowestHalfM > kF31WidthM,
        "**AND THE CAR FITS ON THE NARROWEST STRETCH OF ITS OWN ROUTE.** The harvest already refused "
        "ways narrower than the car; this says the route it chose kept that true end to end");

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
  say.Number("stations whose road kind declares no maximum grade", (double)undeclared, "stations");
  say.Claim(undeclared == 0,
        "**AND EVERY KIND ON THE ROUTE DECLARES ITS OWN MAXIMUM GRADE.** A station with none would be "
        "flattened by a shaping that had nothing to shape it to -- silently, which is the failure "
        "this count exists to make loud. The grades are RAA, RAL and RASt figures declared beside "
        "the widths in src/assets/world/vegetation.json");
  say.Number("the gentlest grade any road class on this route declares", gentlestLimit * 100.0, "%");
  say.Number("the steepest the F31's drivetrain could climb from rest", 23.43257, "%");
  say.Number("the steepest it could hold at its own top speed", 0.15, "%");
  say.Number("where that is", gentlestAtM / 1000.0, "km");

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
  say.Number("sweeps the shaping needed", (double)shapingPasses, "sweeps");

  double cutM = 0.0, fillM = 0.0, cutAtM = 0.0, fillAtM = 0.0, movedM = 0.0;
  for (size_t post = 0; post < roadM.size(); ++post) {
    const double byM = roadM[post] - heightM[post];
    movedM += std::fabs(byM);
    if (byM < cutM) { cutM = byM; cutAtM = (double)post * spanM; }
    if (byM > fillM) { fillM = byM; fillAtM = (double)post * spanM; }
  }
  say.Number("the deepest the road cuts into the ground", -cutM, "m");
  say.Number("where that is", cutAtM / 1000.0, "km");
  say.Number("the highest it fills above it", fillM, "m");
  say.Number("where that is", fillAtM / 1000.0, "km");
  say.Number("the mean earth moved per station", movedM / (double)roadM.size(), "m");
  say.Number("stations still being shaped when the passes ran out", (double)shaped, "stations");

  outshine::Envelope planning = stood.Envelope;
  holdWithinM = 0.5 * narrowestLaneM - 0.5 * kF31WidthM;
  planning.ReserveMs2 = 2.0 * holdWithinM / (1.0 * 1.0);
  const double floorRatio = 1.409 / 0.477;
  planning.HoldWithinM = holdWithinM / floorRatio;
  say.Number("what the negative control measured the closed loop to cost over the first-order lag",
       floorRatio, "x");
  say.Number("so the budget the plan is given", planning.HoldWithinM, "m");
  planning.SettleS = 1.0;
  planning.CorneringNPerRad = declared.Vehicles[0].CorneringNPerRad;
  say.Number("the narrowest road on the route", reaped.NarrowestTakenM, "m");
  say.Number("what the car leaves either side of itself there", holdWithinM, "m");
  say.Number("the lateral acceleration reserved for holding the line", planning.ReserveMs2, "m/s2");
  say.Number("what is left for the path", planning.HoldingMs2(), "m/s2");

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
  if (!rose) { say.Say(Line("REFUSED %s", error.c_str())); }
  say.Number("height knots fastened to the corridor", (double)rise.size(), "knots");
  say.Number("the steepest gradient anywhere on it", worstGradeM, "m/m");
  say.Number("as a percentage", worstGradeM * 100.0, "%");
  say.Number("where that is", worstGradeAtM / 1000.0, "km");

  const double driveN = 400.0 * 3.08 / 0.333;
  const double climbLimit = driveN / (1610.0 * 9.80665);
  say.Number("the steepest the F31's drivetrain can climb", climbLimit * 100.0, "%");
  say.Claim(rose, "**AND THE CORRIDOR RISES WITH THE REAL GROUND UNDER IT.** 8022 heights from the "
              "declared elevation source, each a knot with its own slope, and one cubic through "
              "them -- the same mechanism the synthetic road used, fed by the world");
  say.Claim(std::fabs(worstGradeM) < climbLimit,
        "**AND NOTHING ON IT IS STEEPER THAN THE CAR CAN CLIMB.** 23.4 % is what 3699 N against "
        "15789 N of weight allows; a gradient past that is the drivetrain REFUSING, and on this "
        "route there is none -- which is the first evidence that the ground under an OSM road is "
        "reconstructed well enough to drive");

  const double shortestCornerM = 1.5 * tightestM * 0.1;
  const double profileStepM = 0.5 * shortestCornerM;
  say.Number("the shortest corner the fit can produce", shortestCornerM, "m");
  say.Number("the step the speed profile is sampled at", profileStepM, "m");
  say.Claim(profile.Over(corridor, planning, profileStepM, 0.0, error),
        "and a speed profile is solved over the whole corridor from its geometry alone");
  if (!error.empty()) { say.Say(Line("REFUSED %s", error.c_str())); }

  double slowestMs = 1.0e9, fastestMs = 0.0, meanMs = 0.0;
  for (size_t sample = 0; sample < profile.SampleCount(); ++sample) {
    const double ms = profile.SampleAt(sample);
    slowestMs = ms < slowestMs ? ms : slowestMs;
    fastestMs = ms > fastestMs ? ms : fastestMs;
    meanMs += ms;
  }
  if (profile.SampleCount() > 0) { meanMs /= (double)profile.SampleCount(); }
  say.Number("the slowest the profile asks for", slowestMs * 3.6, "km/h");
  say.Number("the fastest", fastestMs * 3.6, "km/h");
  say.Number("the mean", meanMs * 3.6, "km/h");
  say.Number("stations where a CREST and not a curve set the speed",
       (double)profile.CrestsThatBound(), "stations");
  say.Number("the slowest a crest holds it to", profile.CrestHeldMs() * 3.6, "km/h");
  say.Number("where that crest is", profile.CrestHeldAtM() / 1000.0, "km");
  say.Number("the drive time that implies", fitted.LengthM / (meanMs > 0.0 ? meanMs : 1.0) / 3600.0, "h");


  auto &rig = S_->Rig;
  auto &body = S_->Body;
  rig = stood.Rig;
  body = outshine::Physics::Body();
  body.MassKg = stood.Envelope.MassKg;
  for (int axis = 0; axis < 3; ++axis) {
    body.InertiaKgM2[axis] = declared.Vehicles[0].InertiaKgM2[axis];
  }
  outshine::Placed start;
  say.Claim(corridor.At(0.0, start), "the corridor answers at its own start");
  const outshine::Standing under0 =
      outshine::Stand(corridor, start.EastM, start.NorthM, 0.0, 0.0, 50.0);
  const double startAsideM = asideM.empty() ? 0.0 : asideM.front();
  say.Number("the fastest the car may move between lane centres",
       ((0.5 * narrowestLaneM - 0.5 * kF31WidthM) / (1.0 * stood.Envelope.TopMs())) * 1000.0,
       "mm per metre");
  body.PositionM[0] = start.EastM - std::sin(start.HeadingRad) * startAsideM;
  body.PositionM[1] = under0.HeightM + stood.CentreM[1];
  body.PositionM[2] = -(start.NorthM + std::cos(start.HeadingRad) * startAsideM);
  say.Number("how far from the centreline the car starts, in its own lane", startAsideM, "m");
  {
    const double up[3] = {under0.NormalM[0], under0.NormalM[1], -under0.NormalM[2]};
    Lie(body, start, up);
    const double aheadBody[3] = {0.0, 0.0, -1.0};
    double ahead[3];
    outshine::Physics::Turn(body.OrientationQ, aheadBody, ahead);
    for (int axis = 0; axis < 3; ++axis) { body.VelocityMs[axis] = kJoinMs * ahead[axis]; }
  }

  outshine::Pilot::Reins reins;
  reins.SettleS = 1.0;
  reins.LeastReachM = stood.Axles.WheelbaseM;
  reins.HoldWithinM = holdWithinM;

  S_->AsideRatePerM = (0.5 * narrowestLaneM - 0.5 * kF31WidthM) / (1.0 * stood.Envelope.TopMs());
  
  S_->Ready = true;
  return true;
}

Ridden Journey::Ride(double dtS) {
  Ridden &out = S_->Tally;
  out.Found = false;
  if (!S_->Ready) { return out; }
  auto &corridor = S_->Corridor;
  auto &profile = S_->Profile;
  auto &declared = S_->Declared;
  auto &stood = S_->Stood;
  auto &rig = S_->Rig;
  auto &body = S_->Body;
  auto &fineAside = S_->FineAside;
  auto &fineEdge = S_->FineEdge;
  auto &laneHalfM = S_->LaneHalfM;
  const double fineM = S_->FineM;
  const double spanM = S_->SpanM;
  outshine::Pilot::Reins reins;
  reins.SettleS = 1.0;
  reins.LeastReachM = stood.Axles.WheelbaseM;
  reins.HoldWithinM = S_->HoldWithinM;
  const double gravity[3] = {0.0, -9.80665, 0.0};
  const double dragArea = declared.Vehicles[0].DragCoefficient * declared.Vehicles[0].FrontalM2;
  out.Found = true;

    const double eastM = body.PositionM[0];
    const double northM = -body.PositionM[2];
    const double headingRad = HeadingOf(body);
    const double windowM = kResectM + 3.0 * S_->LostM;
    const outshine::Pilot::Placement at = outshine::Pilot::Locate(
        corridor, eastM, northM, body.PositionM[1], headingRad, S_->NearM, windowM);
    if (!at.Found) {
      out.Lost = true;
      return out;
    }
    S_->NearM = at.AlongM;
    S_->LostM = std::fabs(at.OffsetM);
    out.ReachedM = at.AlongM;

    const double speedMs = std::sqrt(body.VelocityMs[0] * body.VelocityMs[0] +
                                     body.VelocityMs[2] * body.VelocityMs[2]);
    reins.TightestPerM = outshine::Pilot::TightestPerM(stood.Axles, stood.Envelope, speedMs);
    {
      const size_t fine = (size_t)(at.AlongM / fineM);
      const double wantAsideM = fineAside[fine < fineAside.size() ? fine : fineAside.size() - 1];
      if (!S_->HaveAside) {
        S_->HeldAsideM = wantAsideM;
        S_->HaveAside = true;
      } else {
        const double mayMoveM = S_->AsideRatePerM * speedMs * dtS;
        const double byM = wantAsideM - S_->HeldAsideM;
        S_->HeldAsideM += std::fabs(byM) <= mayMoveM ? byM : (byM > 0.0 ? mayMoveM : -mayMoveM);
      }
      const double roomM = fineEdge[fine < fineEdge.size() ? fine : fineEdge.size() - 1] -
                           0.5 * kF31WidthM - S_->BudgetM;
      if (roomM > 0.0) {
        if (S_->HeldAsideM > roomM) { S_->HeldAsideM = roomM; }
        if (S_->HeldAsideM < -roomM) { S_->HeldAsideM = -roomM; }
      }
      reins.AsideM = S_->HeldAsideM;
    }
    const double brakingM =
        speedMs * speedMs / (2.0 * (stood.Envelope.BrakeMs2() > 0.0 ? stood.Envelope.BrakeMs2()
                                                                    : 1.0));
    double wantedMs = profile.At(at.AlongM);
    double needMs2 = 0.0;
    for (int look = 1; look <= 12; ++look) {
      const double overM = brakingM * (double)look / 12.0;
      const double atM = std::fmin(at.AlongM + overM, corridor.LengthM());
      const double thereMs = profile.At(atM);
      if (thereMs < speedMs && overM > 0.0) {
        const double askMs2 = (speedMs * speedMs - thereMs * thereMs) / (2.0 * overM);
        if (askMs2 > needMs2) { needMs2 = askMs2; }
      }
      if (thereMs < wantedMs) { wantedMs = thereMs; }
    }
    outshine::Pilot::Demand asked = Hold(corridor, reins, at, speedMs, wantedMs);
    if (needMs2 > 0.0 && -needMs2 < asked.AlongMs2) { asked.AlongMs2 = -needMs2; }
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
      const size_t band = (size_t)(at.AlongM / fineM) < fineEdge.size()
                              ? (size_t)(at.AlongM / fineM)
                              : fineEdge.size() - 1;
      const double edgeM = fineEdge[band];
      const outshine::Standing on =
          outshine::Stand(corridor, worldM[0], -worldM[2], 0.0, at.AlongM,
                          kMountResectM + 3.0 * S_->LostM);
      const double armAcrossM =
          -std::sin(headingRad) * (worldM[0] - eastM) + std::cos(headingRad) * (-worldM[2] - northM);
      under[which].Found = std::fabs(at.OffsetM + armAcrossM) <= edgeM;
      under[which].HeightM = on.HeightM;
      under[which].NormalM[0] = on.NormalM[0];
      under[which].NormalM[1] = on.NormalM[1];
      under[which].NormalM[2] = -on.NormalM[2];
    }

    outshine::Physics::Wrench wrench;
    outshine::Physics::Fall(wrench, body, gravity);
    outshine::Physics::Resist(wrench, body, dragArea, declared.Vehicles[0].AirDensity);
    const outshine::Physics::Reading read =
        outshine::Physics::Bear(rig, body, under, controls, wrench, dtS);

    if (at.AlongM >= kFromM) {
      const double inLaneM = at.OffsetM - reins.AsideM;
      if (std::fabs(inLaneM) > std::fabs(out.WorstOffsetM)) {
        out.WorstOffsetM = inLaneM;
        out.WorstOffsetAtM = at.AlongM;
      }
      out.WorstRatio = std::fmax(out.WorstRatio, read.WorstRatio);
      out.TopMs = std::fmax(out.TopMs, speedMs);
      if (read.PastLimit && !out.PastLimit) {
        out.PastLimit = true;
        out.BrokeAtM = at.AlongM;
      }
      out.PastTravel = out.PastTravel || read.PastTravel;
      if (read.Airborne > out.MostAirborne) {
        out.MostAirborne = read.Airborne;
        out.AirborneAtM = at.AlongM;
      }
    }

    if (read.OffTheSurface > 0 && out.LeftTheRoadAtM <= 0.0) {
      out.LeftTheRoadAtM = at.AlongM;
      out.LeftByM = at.OffsetM - reins.AsideM;
      out.LeftAtMs = speedMs;
      out.LeftPlannedMs = profile.At(at.AlongM);
      out.LeftCurvature = at.CurvaturePerM;
      out.LeftRate = at.CurvatureRatePerM;
      const size_t post = (size_t)(at.AlongM / spanM);
      out.LeftLaneM = 2.0 * laneHalfM[post < laneHalfM.size() ? post : laneHalfM.size() - 1];
      const size_t fine = (size_t)(at.AlongM / fineM);
      out.LeftEdgeM = fineEdge[fine < fineEdge.size() ? fine : fineEdge.size() - 1];
      out.LeftAsideM = reins.AsideM;
      out.LeftAcrossM = at.OffsetM;
    }
    if (read.OffTheSurface > 0) {
      out.BrokeAtM = at.AlongM;
      out.LeftTheRoadAtM = at.AlongM;
      return out;
    }
    if (read.PastLimit || read.Airborne == rig.Count) {
      out.BrokeAtM = at.AlongM;
      out.PastLimit = out.PastLimit || read.PastLimit;
      out.MostAirborne = read.Airborne;
      return out;
    }
    outshine::Physics::Step(body, wrench, dtS);
    S_->SimulatedS += dtS;
    out.SimulatedS = S_->SimulatedS;
    if (at.AlongM >= corridor.LengthM() - 20.0) { out.Arrived = true; }
  return out;
}

} // namespace outshine::Driver
