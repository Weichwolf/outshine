#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "DriveAssembly.h"

#include "Body.h"
#include "Carriageway.h"
#include "Units.h"
#include "TileGeodesy.h"
#include "Drive.h"
#include "Fit.h"
#include "Transport.h"
#include "GroundStack.h"
#include "OsmField.h"
#include "RoadHarvest.h"
#include "TerrainLoader.h"
#include "TilePool.h"
#include "VegetationTemplates.h"
#include "Wayfinding.h"

using outshine::Path::ApartM;
using outshine::Path::Network;
using outshine::Ground::OsmField;
using outshine::Ground::OsmLayer;
using outshine::Ground::OsmLayerNames;
using outshine::Ground::Reap;
using outshine::Ground::Reaped;
using outshine::Path::Route;
using outshine::Ground::VegetationTemplates;
using outshine::Path::Waypoint;

namespace outshine::Sim {

namespace {
// [SET] the corridor build's patience, s: a worldwide route needs thousands of tiles
// over a live wire; 900 s is the measured Munich--Hamburg cold-cache build doubled
constexpr double kPatienceS = 900.0;
// [SET] the join speed, m/s: the car enters its corridor rolling (72 km/h) rather than
// standing, so the first tallied seconds exercise the drive and not the launch
constexpr double kJoinMs = 20.0;
} // namespace

bool AssembleDrive(const Store &scene, const Assembled &cast, const Column<Vehicle> &vehicles,
                   const Column<Drive> &driven, const WorldSettings &world,
                   Ground::GroundStack &stack, Data::Transport &wire, const Provision &kept,
                   Sink &say, DriveProduct &out) {
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
  out.Car = *car;

  auto &corridor = out.Way.Line;
  auto &stood = out.Stood;
  auto &asideM = out.Way.AsideM;
  auto &narrowestLaneM = out.Way.NarrowestLaneM;


  const double straightM = ApartM(fromLatDeg, fromLonDeg, toLatDeg, toLonDeg, world.RadiusM);
  const double middleLat = 0.5 * (fromLatDeg + toLatDeg);
  const double middleLon = 0.5 * (fromLonDeg + toLonDeg);
  say.Number("start to destination as the crow flies", straightM / 1000.0, "km");
  say.Number("the zoom the ways are read at", (double)kZoom, "");
  const double tileGroundM =
      outshine::Ground::kMercatorGirthM * std::cos(middleLat * outshine::kPi / 180.0) /
      (double)(1L << kZoom);
  // [SET] tiles fetched around each route step: ring 2 is the 5x5 patch -- one tile of
  // slack past the corridor's widest swing at this zoom, and the admission bound
  const int kCorridorRing = 2;
  const long steps = (long)std::ceil(straightM / tileGroundM) + 1;
  const long square = (long)(2 * std::ceil(0.5 * straightM / tileGroundM) + 3);
  say.Number("a tile's ground size at this zoom and latitude", tileGroundM / 1000.0, "km");
  say.Number("stations along the line the corridor is fetched at", (double)steps, "stations");
  say.Number("the ring fetched around each", (double)(2 * kCorridorRing + 1), "tiles across");
  say.Number("what a square covering both cities would have cost", (double)(square * square), "tiles");

  std::string error;
  out.State.CarWidthM = out.Car.WidthM;
  const double carWidthM = out.State.CarWidthM;
  say.Number("the width the declaration gives the car", carWidthM, "m");
  say.Claim(carWidthM > 0.0,
        "**THE CAR'S WIDTH IS THE DECLARATION'S**, because every lane judgement stands on it -- "
        "a vehicle without a declared width cannot be fitted to any road");
  if (!(carWidthM > 0.0)) { return false; }

  if (!stack.Open(kept.CacheDir, kept.AssetsDir, middleLat, middleLon, wire, say)) {
    return false;
  }

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
      built += field.Build(stack.Pool(), atLat, atLon, kCorridorRing);
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

  outshine::Ground::GroundMaterials materials;
  say.Claim(materials.Load((kept.AssetsDir + "/world/ground-materials.json").c_str()),
        "the declared ground materials load, because a width table names them");
  VegetationTemplates widths;
  say.Claim(widths.Load((kept.AssetsDir + "/world/vegetation.json").c_str(), materials),
        "the declared widths load, with their RAA, RAL and RASt origins");

  const double quantumM = outshine::Ground::kMercatorGirthM / ((double)(1L << kZoom) * 4096.0);
  Network roads(1.05 * quantumM, world.RadiusM);
  say.Number("the tile's own coordinate quantisation", quantumM, "m");
  const Reaped reaped = Reap(field, widths, carWidthM, roads);
  say.Claim(!reaped.StreetsAbsent,
        "the streets layer is present -- an absent layer and an empty one are different "
        "facts, and every count below would otherwise be an honest-looking zero");
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
        "car on the tracks. What a CARRIAGEWAY has is LANES; a railway declares none, and that is "
        "the test");
  say.Number("ways whose kind declares no maximum grade", (double)reaped.Ungraded, "ways");

  if (!reaped.WithoutGrade.empty()) {
    say.Say(Line("UNGRADED KINDS %s", reaped.WithoutGrade.c_str()));
  }
  say.Claim(reaped.Ways > 0,
        Line("and roads a %s m car can fit down are harvested from them",
             std::to_string(out.Car.WidthM)).c_str());
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

  stood = outshine::Sim::Stand(out.Car, world.GravityMs2, world.AirDensityKgM3);
  if (!stood.Stood) { say.Say(Line("REFUSED %s", stood.Error.c_str())); }
  say.Claim(stood.Stood, "**AND THE DECLARED VEHICLE STANDS UP AS A RIG.** Every number the drive uses comes "
                     "from the file, not from a constant beside it");
  if (!stood.Stood) { return false; }
  const double tightestM = stood.TightestM;
  say.Number("the tightest centreline circle the declaration implies", tightestM, "m");
  const Route route =
      roads.Plan(Waypoint{fromLatDeg, fromLonDeg}, Waypoint{toLatDeg, toLonDeg}, tightestM);
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
  if (!Sim::LayCorridor(route, stack.Ground(), out.Car, stood, quantumM, tightestM,
                        middleLat, world.RadiusM, say, out.Way, error)) {
    return false;
  }

  auto &rig = out.State.Rig;
  auto &body = out.State.Body;
  rig = stood.Rig;
  body = outshine::Physics::Body();
  body.MassKg = stood.Envelope.MassKg;
  for (int axis = 0; axis < 3; ++axis) {
    body.InertiaKgM2[axis] = out.Car.InertiaKgM2[axis];
  }
  outshine::Placed start;
  say.Claim(corridor.At(0.0, start), "the corridor answers at its own start");
  const outshine::Standing under0 =
      // [SET] 50 m of search: the stand-at node came off the same corridor, so the
      // nearest laid station is within one node spacing of it
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

  out.State.AsideRatePerM = (0.5 * narrowestLaneM - 0.5 * carWidthM) / (1.0 * stood.Envelope.TopMs());
  
  out.Ready = true;
  return true;
}
}
