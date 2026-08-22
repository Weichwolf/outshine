#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "Journey.h"

#include "Carriageway.h"
#include "Ribbon.h"
#include "ContentStore.h"
#include "Drive.h"
#include "CorridorLay.h"
#include "Rigging.h"
#include "ScenarioRead.h"
#include "Fit.h"
#include "Transport.h"
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

constexpr double kPatienceS = 900.0;
constexpr double kJoinMs = 20.0;




} // namespace


namespace outshine::Sim {

struct Journey::State {
  std::unique_ptr<outshine::Data::ContentStore> Store;
  std::unique_ptr<outshine::Data::SourceSet> Sources;
  std::unique_ptr<outshine::World::TilePool> Pool;
  std::unique_ptr<outshine::World::GroundStream> Ground;
  outshine::Vehicle Car;
  outshine::Sim::Rigged Stood;
  outshine::Sim::Corridor Way;
  outshine::Sim::DriveState Drive;
  bool Opened = false;
  bool Ready = false;
};

Journey::Journey() : S_(std::make_unique<State>()) {}
Journey::~Journey() { Close(); }

void Journey::Close(void) {
  if (S_ && S_->Opened) {
    S_->Ground.reset();
    S_->Pool.reset();
    S_->Opened = false;
  }
}

const outshine::Physics::Body &Journey::Carried(void) const { return S_->Drive.Body; }
outshine::World::GroundStream &Journey::Ground(void) const { return *S_->Ground; }

const outshine::ReferenceLine &Journey::Corridor(void) const { return S_->Way.Line; }
double Journey::LengthM(void) const { return S_->Way.Line.LengthM(); }

double Journey::ReserveMs2(void) const { return S_->Way.ReserveMs2; }

void Journey::Frame(double &latDeg, double &lonDeg, double &perLatM, double &perLonM) const {
  latDeg = S_->Way.FrameLat;
  lonDeg = S_->Way.FrameLon;
  perLatM = S_->Way.PerLatM;
  perLonM = S_->Way.PerLonM;
}

bool Journey::Lay(const Store &scene, const Assembled &cast, const Column<Vehicle> &vehicles,
                  const Column<Drive> &driven, const WorldSettings &world, Data::Transport &wire,
                  const Provision &kept, Sink &say) {
  say.Claim(!kept.CacheDir.empty() && !kept.AssetsDir.empty(),
        "**THE CALLER PROVISIONS THE JOURNEY**: a cache directory and an assets root arrive as "
        "declarations -- the library owns no path");
  if (kept.CacheDir.empty() || kept.AssetsDir.empty()) { return false; }
  const outshine::Vehicle *car = vehicles.Get(cast.PlayerBody);
  say.Claim(car != nullptr,
        "**THE CAR ARRIVES AS A HANDLE THROUGH THE ONE DOOR**: the assembled body carries its "
        "declaration in the column, and a journey without one refuses");
  if (car == nullptr) { return false; }
  const outshine::Drive *driveTo = driven.Get(cast.Assignment);
  const bool assigned =
      driveTo != nullptr && scene.TargetOf(cast.PlayerMind, Relation::Assigned) == cast.Assignment;
  say.Claim(assigned,
        "**AND SO DOES THE ASSIGNMENT**: the mind is Assigned the coordinates this journey "
        "will lay -- two coordinates and a zoom as column data, not parameters");
  if (!assigned) { return false; }
  const double fromLatDeg = driveTo->FromLatDeg;
  const double fromLonDeg = driveTo->FromLonDeg;
  const double toLatDeg = driveTo->ToLatDeg;
  const double toLonDeg = driveTo->ToLonDeg;
  const int kZoom = driveTo->Zoom;
  S_->Car = *car;

  auto &corridor = S_->Way.Line;
  auto &stood = S_->Stood;
  auto &asideM = S_->Way.AsideM;
  auto &narrowestLaneM = S_->Way.NarrowestLaneM;


  const double straightM = ApartM(fromLatDeg, fromLonDeg, toLatDeg, toLonDeg);
  const double middleLat = 0.5 * (fromLatDeg + toLatDeg);
  const double middleLon = 0.5 * (fromLonDeg + toLonDeg);
  say.Number("start to destination as the crow flies", straightM / 1000.0, "km");
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

  std::string error;
  S_->Drive.CarWidthM = S_->Car.WidthM;
  const double carWidthM = S_->Drive.CarWidthM;
  say.Number("the width the declaration gives the car", carWidthM, "m");
  say.Claim(carWidthM > 0.0,
        "**THE CAR'S WIDTH IS THE DECLARATION'S**, because every lane judgement stands on it -- "
        "a vehicle without a declared width cannot be fitted to any road");
  if (!(carWidthM > 0.0)) { return false; }

  outshine::Data::ContentStore::Config keeping;
  keeping.Directory = kept.CacheDir;
  S_->Store = std::make_unique<outshine::Data::ContentStore>(keeping);
  S_->Sources = std::make_unique<outshine::Data::SourceSet>(*S_->Store);
  outshine::Data::SourceSet &sources = *S_->Sources;
  say.Claim(outshine::Data::RegisterDeclared(sources, {kept.AssetsDir + "/sky", true}) ==
            outshine::Data::Registered::Complete,
        "the declared upstream sources register, ranked and without a clash");
  say.Number("sources registered", (double)sources.Count(), "sources");

  outshine::World::GroundSurface surface;
  surface.Z = 12;
  surface.Grid = 64;
  S_->Pool = std::make_unique<outshine::World::TilePool>(
      outshine::World::GroundPoolConfig(middleLat, middleLon), sources, wire);
  S_->Ground = std::make_unique<outshine::World::GroundStream>(*S_->Pool, surface);

  OsmField field(kZoom, OsmLayerNames({OsmLayer::Streets, OsmLayer::StreetPolygons}));
  const auto began = std::chrono::steady_clock::now();
  long passes = 0;
  int built = 0;
  bool ranOut = false;
  for (long step = 0; step <= steps && !ranOut; ++step) {
    const double part = (double)step / (double)steps;
    const double atLat = fromLatDeg + part * (toLatDeg - fromLatDeg);
    const double atLon = fromLonDeg + part * (toLonDeg - fromLonDeg);
    for (;;) {
      built += field.Build(*S_->Pool, atLat, atLon, kCorridorRing);
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
        "declared upstream source is asked for the tiles between start and destination and answers "
        "with vector geometry");
  if (field.Features().empty()) { return false; }

  outshine::World::GroundMaterials materials;
  say.Claim(materials.Load((kept.AssetsDir + "/world/ground-materials.json").c_str()),
        "the declared ground materials load, because a width table names them");
  VegetationTemplates widths;
  say.Claim(widths.Load((kept.AssetsDir + "/world/vegetation.json").c_str(), materials),
        "the declared widths load, with their RAA, RAL and RASt origins");

  const double quantumM = 40075017.0 / ((double)(1L << kZoom) * 4096.0);
  Network roads(1.05 * quantumM);
  say.Number("the tile's own coordinate quantisation", quantumM, "m");
  const Reaped reaped = Reap(field, widths, carWidthM, roads);
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
  say.Claim(reaped.WidestRefusedM < carWidthM || reaped.TooNarrow == 0,
        "**ADMISSIBILITY IS THE VEHICLE'S WIDTH AND NOT A LIST OF TAGS.** Every way this refused is "
        "narrower than the car; every way it took is wider. Nobody wrote down which highway kinds a "
        "car may use, and traffic law stays unmodelled -- what decides is whether it fits");

  say.Claim(roads.Weave(error), "the ways weave into a network");
  if (!error.empty()) { say.Say(Line("REFUSED %s", error.c_str())); }
  say.Number("nodes after snapping", (double)roads.NodeCount(), "nodes");
  say.Number("junctions among them", (double)roads.JunctionCount(), "nodes");
  say.Number("edges", (double)roads.EdgeCount(), "edges");

  size_t atFrom = 0, atTo = 0;
  double fromAwayM = 0.0, toAwayM = 0.0;
  say.Claim(roads.Nearest(Waypoint{fromLatDeg, fromLonDeg}, atFrom, fromAwayM) &&
            roads.Nearest(Waypoint{toLatDeg, toLonDeg}, atTo, toAwayM),
        "both city centres resolve to a node of the network");
  say.Number("how far the start is from the nearest road node", fromAwayM, "m");
  say.Number("how far the destination is from the nearest road node", toAwayM, "m");
  say.Number("how far each walk is as a share of the drive",
       (fromAwayM + toAwayM) / straightM, "of it");

  const outshine::Vehicle &turning = S_->Car;
  const double outerM = turning.TurningCircleM * 0.5;
  const double tightestM =
      std::sqrt(outerM * outerM - turning.WheelbaseM * turning.WheelbaseM);
  say.Number("the tightest centreline circle the declaration implies", tightestM, "m");
  const Route route =
      roads.Plan(Waypoint{fromLatDeg, fromLonDeg}, Waypoint{toLatDeg, toLonDeg}, tightestM,
                 quantumM);
  say.Number("turns the search refused as too sharp for the car", (double)route.TurnsRefused, "turns");
  if (!route.Found) { say.Say(Line("REFUSED %s", route.Error.c_str())); }
  say.Number("nodes the search settled", (double)route.Reached, "nodes");
  say.Claim(route.Found,
        "**AND A ROUTE IS FOUND FROM START TO DESTINATION OVER OSM'S OWN WAYS.** Two "
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


  say.Number("the narrowest road on the route", reaped.NarrowestTakenM, "m");
  stood = outshine::Sim::Stand(S_->Car, world.GravityMs2);
  if (!stood.Stood) { say.Say(Line("REFUSED %s", stood.Error.c_str())); }
  say.Claim(stood.Stood, "**AND THE DECLARED F31 STANDS UP AS A RIG.** Every number the drive uses comes "
                     "from the file, not from a constant beside it");
  if (!stood.Stood) { return false; }

  if (!Sim::LayCorridor(route, *S_->Ground, S_->Car, stood, quantumM, tightestM,
                        middleLat, say, S_->Way, error)) {
    return false;
  }

  auto &rig = S_->Drive.Rig;
  auto &body = S_->Drive.Body;
  rig = stood.Rig;
  body = outshine::Physics::Body();
  body.MassKg = stood.Envelope.MassKg;
  for (int axis = 0; axis < 3; ++axis) {
    body.InertiaKgM2[axis] = S_->Car.InertiaKgM2[axis];
  }
  outshine::Placed start;
  say.Claim(corridor.At(0.0, start), "the corridor answers at its own start");
  const outshine::Standing under0 =
      outshine::Stand(corridor, start.EastM, start.NorthM, 0.0, 0.0, 50.0);
  const double startAsideM = asideM.empty() ? 0.0 : asideM.front();
  say.Number("the fastest the car may move between lane centres",
       ((0.5 * narrowestLaneM - 0.5 * carWidthM) / (1.0 * stood.Envelope.TopMs())) * 1000.0,
       "mm per metre");
  body.PositionM[0] = start.EastM - std::sin(start.HeadingRad) * startAsideM;
  body.PositionM[1] = under0.HeightM + stood.CentreM[1];
  body.PositionM[2] = -(start.NorthM + std::cos(start.HeadingRad) * startAsideM);
  say.Number("how far from the centreline the car starts, in its own lane", startAsideM, "m");
  {
    const double up[3] = {under0.NormalM[0], under0.NormalM[1], -under0.NormalM[2]};
    const double aheadM[3] = {std::cos(start.HeadingRad), start.Slope,
                              -std::sin(start.HeadingRad)};
    outshine::Physics::Lie(body, aheadM, up);
    const double aheadBody[3] = {0.0, 0.0, -1.0};
    double ahead[3];
    outshine::Physics::Turn(body.OrientationQ, aheadBody, ahead);
    for (int axis = 0; axis < 3; ++axis) { body.VelocityMs[axis] = kJoinMs * ahead[axis]; }
  }

  S_->Drive.AsideRatePerM = (0.5 * narrowestLaneM - 0.5 * carWidthM) / (1.0 * stood.Envelope.TopMs());
  
  S_->Ready = true;
  return true;
}

Ridden Journey::Ride(double dtS, const Taken *taken) {
  if (!S_->Ready) {
    S_->Drive.Tally.Found = false;
    return S_->Drive.Tally;
  }
  return Sim::DriveTick(S_->Way, S_->Stood, S_->Car, S_->Drive, dtS, taken);
}

} // namespace outshine::Sim
