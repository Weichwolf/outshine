#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "math/Vec3.h"
#include "DriveAssembly.h"

#include "Rigid.h"
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

using outshine::Ground::OsmField;
using outshine::Ground::OsmLayer;
using outshine::Ground::OsmLayerNames;
using outshine::Ground::Reap;
using outshine::Ground::Reaped;
using outshine::Ground::VegetationTemplates;
using outshine::Path::ApartM;
using outshine::Path::Network;
using outshine::Path::Route;
using outshine::Path::Waypoint;

namespace outshine::Sim {

constexpr double kSpeedMargin = 1.05;

namespace {
constexpr double kPatienceS = 900.0;
constexpr double kJoinMs = 20.0;
} // namespace

bool AssembleDrive(const Scene &scene,
                   const Assembled &cast,
                   const Column<Scenario::Body> &bodies,
                   const Column<Scenario::Journey> &driven,
                   const Scenario::WorldSettings &world,
                   Ground::GroundStack &stack,
                   Data::Transport &wire,
                   const Provision &kept,
                   Sink &say,
                   DriveProduct &out) {
  if (kept.CacheDir.empty() || kept.AssetsDir.empty()) {
    say.Refuse("no cache directory or assets root was provisioned");
    return false;
  }
  const outshine::Scenario::Body *car = bodies.Get(cast.PlayerBody);
  if (car == nullptr) {
    say.Refuse("the assembled body carries no body declaration");
    return false;
  }
  const outshine::Scenario::Journey *driveTo = driven.Get(cast.Assignment);
  const bool assigned =
      driveTo != nullptr && scene.targetOf(cast.PlayerMind, Relation::Assigned) == cast.Assignment;
  if (!assigned) { say.Refuse("the mind carries no assignment"); }
  if (!assigned) { return false; }
  const double fromLatDeg = driveTo->FromLatDeg;
  const double fromLonDeg = driveTo->FromLonDeg;
  const double toLatDeg = driveTo->ToLatDeg;
  const double toLonDeg = driveTo->ToLonDeg;
  out.Car = *car;

  const auto &corridor = out.Way.Line;
  auto &stood = out.Stood;

  const double straightM = ApartM(fromLatDeg, fromLonDeg, toLatDeg, toLonDeg, world.Origin.RadiusM);
  const double middleLat = 0.5 * (fromLatDeg + toLatDeg);
  const double middleLon = 0.5 * (fromLonDeg + toLonDeg);
  say.Number("start to destination as the crow flies", straightM / kMPerKm, "km");
  std::string error;
  out.State.CarWidthM = out.Car.WidthM;
  const double carWidthM = out.State.CarWidthM;
  say.Number("the width the declaration gives the car", carWidthM, "m");
  if (!(carWidthM > 0.0)) {
    say.Refuse("the body declares no width");
    return false;
  }

  if (!stack.Open(kept.CacheDir, kept.AssetsDir, kept.Providers, middleLat, middleLon, wire, say)) {
    return false;
  }

  const int kZoom = stack.FinestZoomOf(Data::DataKind::VectorMap);
  if (kZoom <= 0) {
    say.Refuse("no declared source carries a vector map, so there is no zoom to read the ways "
               "at");
    return false;
  }
  say.Number("the zoom the ways are read at", static_cast<double>(kZoom), "");

  const double tileGroundM = outshine::Ground::kMercatorGirthM *
                             std::cos(middleLat * outshine::kPi / kDegPerHalfTurn) /
                             static_cast<double>(1L << static_cast<uint32_t>(kZoom));
  const int kCorridorRing = 2;
  const long steps = static_cast<long>(std::ceil(straightM / tileGroundM)) + 1;
  const long square = static_cast<long>(2 * std::ceil(0.5 * straightM / tileGroundM) + 3);
  say.Number("a tile's ground size at this zoom and latitude", tileGroundM / kMPerKm, "km");
  say.Number(
      "stations along the line the corridor is fetched at", static_cast<double>(steps), "stations");
  say.Number(
      "the ring fetched around each", static_cast<double>(2 * kCorridorRing + 1), "tiles across");
  say.Number("what a square covering both cities would have cost",
             static_cast<double>(square * square),
             "tiles");

  OsmField field(kZoom, OsmLayerNames({OsmLayer::Streets, OsmLayer::StreetPolygons}));
  const auto began = std::chrono::steady_clock::now();
  long passes = 0;
  int built = 0;
  bool ranOut = false;
  for (long step = 0; step <= steps && !ranOut; ++step) {
    const double part = static_cast<double>(step) / static_cast<double>(steps);
    const double atLat = fromLatDeg + part * (toLatDeg - fromLatDeg);
    const double atLon = fromLonDeg + part * (toLonDeg - fromLonDeg);
    for (;;) {
      built += field.Build(stack.Pool(), atLat, atLon, kCorridorRing, 0.0);
      ++passes;
      if (field.PendingTiles() == 0) { break; }
      if (std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count() >
          kPatienceS) {
        ranOut = true;
        break;
      }
    }
  }
  out.Found.RanOutOfPatience = ranOut;
  const double fetchedS =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();

  say.Number("passes over the corridor", static_cast<double>(passes), "passes");
  say.Number(
      "tiles the corridor actually took", static_cast<double>(field.Tiles().size()), "tiles");
  say.Number("tiles decoded", static_cast<double>(built), "tiles");
  say.Number("tiles still pending", static_cast<double>(field.PendingTiles()), "tiles");
  say.Number("tiles the sources refused", static_cast<double>(field.RefusedTiles()), "tiles");
  say.Number(
      "layers the server did not send", static_cast<double>(field.MissingLayers()), "layers");
  say.Number("tiles that would not decode", static_cast<double>(field.BadTiles()), "tiles");
  say.Number("features read", static_cast<double>(field.Features().size()), "features");
  say.Number("points in them", static_cast<double>(field.Points().size() / 2), "points");
  out.Found.FetchedS = fetchedS;
  say.Number("seconds spent fetching and decoding", fetchedS, "s");

  out.Found.Features = static_cast<long>(field.Features().size());
  if (field.Features().empty()) {
    say.Refuse("the fetched tiles decode to no feature");
    return false;
  }

  outshine::Ground::GroundMaterials materials;
  if (!materials.Load((kept.AssetsDir + "/world/ground-materials.json").c_str())) {
    say.Refuse("the declared ground materials do not load");
    return false;
  }
  VegetationTemplates &widths = out.Surfaces;
  if (!widths.Load((kept.AssetsDir + "/world/vegetation.json").c_str(), materials)) {
    say.Refuse("the declared width table does not load");
    return false;
  }
  stack.SetVegetation(&widths);

  const double quantumM = outshine::Ground::kMercatorGirthM /
                          (static_cast<double>(1L << static_cast<uint32_t>(kZoom)) * 4096.0);
  Network roads(kSpeedMargin * quantumM, world.Origin.RadiusM);
  say.Number("the tile's own coordinate quantisation", quantumM, "m");
  const Reaped reaped = Reap(field, widths, carWidthM, roads);
  out.Found.StreetsAbsent = reaped.StreetsAbsent;
  say.Number("the snapping distance just above it", roads.SnapM(), "m");
  say.Number("ways a car can fit down", static_cast<double>(reaped.Ways), "ways");
  say.Number("points in them", static_cast<double>(reaped.Points), "points");
  say.Number("ways too narrow for it", static_cast<double>(reaped.TooNarrow), "ways");
  say.Number("the widest way it refused", reaped.WidestRefusedM, "m");
  say.Number("the narrowest it took", reaped.NarrowestTakenM, "m");
  say.Number(
      "ways whose kind carries no declared width", static_cast<double>(reaped.Unclassed), "ways");

  say.Number("ways that are not a carriageway at all",
             static_cast<double>(reaped.NotACarriageway),
             "ways");
  say.Number("ways the data marks as a bridge", static_cast<double>(reaped.Bridges), "ways");
  say.Number("ways it marks as a tunnel", static_cast<double>(reaped.Tunnels), "ways");
  say.Number("ways that declare a stacking layer", static_cast<double>(reaped.Layered), "ways");
  say.Number("the deepest layer any of them names", reaped.DeepestLayer, "");
  say.Number("the highest", reaped.HighestLayer, "");
  say.Number(
      "distinct tag keys the vector tiles carry", static_cast<double>(field.KeyCount()), "keys");
  for (size_t at = 0; at < field.KeyCount(); ++at) {
    say.Say(Line("KEY %s", std::string(field.KeyAt(at)).c_str()));
  }
  if (!reaped.NotCarriageways.empty()) {
    say.Say(Line("NOT CARRIAGEWAYS %s", reaped.NotCarriageways.c_str()));
  }
  out.Found.NotACarriageway = static_cast<long>(reaped.NotACarriageway);
  say.Number(
      "ways whose kind declares no maximum grade", static_cast<double>(reaped.Ungraded), "ways");

  if (!reaped.WithoutGrade.empty()) {
    say.Say(Line("UNGRADED KINDS %s", reaped.WithoutGrade.c_str()));
  }
  out.Found.Ways = static_cast<long>(reaped.Ways);
  out.Found.TooNarrow = static_cast<long>(reaped.TooNarrow);
  out.Found.Ungraded = static_cast<long>(reaped.Ungraded);
  out.Found.WidestRefusedM = reaped.WidestRefusedM;

  const size_t joined = roads.Cross();
  say.Number("crossings at grade made into junctions", static_cast<double>(joined), "crossings");
  say.Number("crossings left alone because one way spans",
             static_cast<double>(roads.CrossingsLeftAlone()),
             "crossings");
  if (!roads.Weave(error)) {
    say.Refuse(Line("the ways do not weave into a network: %s", error.c_str()));
    return false;
  }
  out.Found.Nodes = static_cast<long>(roads.NodeCount());
  out.Found.Junctions = static_cast<long>(roads.JunctionCount());
  say.Number("nodes after snapping", static_cast<double>(out.Found.Nodes), "nodes");
  say.Number("junctions among them", static_cast<double>(out.Found.Junctions), "nodes");
  say.Number("edges", static_cast<double>(roads.EdgeCount()), "edges");
  say.Number(
      "loose ends tied onto an edge they end on", static_cast<double>(roads.TiedToEdges()), "ends");
  say.Number("cells the tie index holds", static_cast<double>(roads.CellsInTheTieIndex()), "cells");
  {
    const Network::Pieces broken = roads.InPieces();
    say.Number("pieces the graph falls into", static_cast<double>(broken.Count), "pieces");
    say.Number("nodes in the largest", static_cast<double>(broken.Largest), "nodes");
    say.Number(
        "pieces holding fewer than four nodes", static_cast<double>(broken.UnderFour), "pieces");
    say.Number("nodes stranded in those", static_cast<double>(broken.InUnderFour), "nodes");
  }
  {
    std::vector<Path::Network::Crossing> crossings;
    const auto sweep = roads.Crossings(crossings);
    if (!sweep) {
      say.Refuse(Line("the crossing sweep cannot grid this network: %s",
                      std::string(sweep.error()).c_str()));
      return false;
    }
    const Network::Swept swept = *sweep;
    const size_t found = swept.Found;
    say.Number("places two ways cross in plan without sharing a node",
               static_cast<double>(found),
               "places");
    out.Found.Crossings = static_cast<long>(found);
    say.Number(
        "segment pairs the crossing sweep tested", static_cast<double>(swept.PairsTested), "pairs");
    say.Number("segments in the cell that held the most",
               static_cast<double>(swept.FullestCell),
               "segments");
  }

  size_t atFrom = 0;
  size_t atTo = 0;
  double fromAwayM = 0.0;
  double toAwayM = 0.0;
  if (!roads.Nearest(
          Waypoint{.LatitudeDeg = fromLatDeg, .LongitudeDeg = fromLonDeg}, atFrom, fromAwayM) ||
      !roads.Nearest(Waypoint{.LatitudeDeg = toLatDeg, .LongitudeDeg = toLonDeg}, atTo, toAwayM)) {
    say.Refuse("a waypoint resolves to no node of the network");
    return false;
  }
  out.Found.FromAwayM = fromAwayM;
  out.Found.ToAwayM = toAwayM;
  say.Number("how far the start is from the nearest road node", fromAwayM, "m");
  say.Number("how far the destination is from the nearest road node", toAwayM, "m");
  say.Number(
      "how far each walk is as a share of the drive", (fromAwayM + toAwayM) / straightM, "of it");

  stood = outshine::Sim::Stand(out.Car, world.GravityMs2, world.AirDensityKgM3);
  if (!stood.Stood) { say.Refuse(Line("%s", stood.Error.c_str())); }
  if (!stood.Stood) { return false; }
  const double tightestM = stood.TightestM;
  say.Number("the tightest centreline circle the declaration implies", tightestM, "m");
  const Route route = roads.Plan(Waypoint{.LatitudeDeg = fromLatDeg, .LongitudeDeg = fromLonDeg},
                                 Waypoint{.LatitudeDeg = toLatDeg, .LongitudeDeg = toLonDeg},
                                 tightestM);
  say.Number("turns the search refused as too sharp for the car",
             static_cast<double>(route.TurnsRefused),
             "turns");
  if (!route.Found) { say.Refuse(Line("%s", route.Error.c_str())); }
  say.Number("nodes the search settled", static_cast<double>(route.Reached), "nodes");
  if (!route.Found) { return false; }
  out.Found.TurnsRefused = static_cast<long>(route.TurnsRefused);

  say.Number("how far the route runs", route.LengthM / kMPerKm, "km");
  say.Number("how far the crow flies", route.StraightM / kMPerKm, "km");
  say.Number("the detour that is", route.LengthM / route.StraightM, "x");
  out.Found.RouteLengthM = route.LengthM;
  out.Found.StraightM = route.StraightM;
  say.Number("legs in it", static_cast<double>(route.Legs.size()), "legs");

  say.Number("the narrowest road on the route", reaped.NarrowestTakenM, "m");
  if (!Sim::LayCorridor(route,
                        stack.Ground(),
                        out.Car,
                        stood,
                        quantumM,
                        tightestM,
                        middleLat,
                        world.Origin.RadiusM,
                        say,
                        out.Way,
                        error)) {
    return false;
  }

  out.Way.AsideFriction =
      static_cast<double>(widths.FrictionOf(static_cast<size_t>(widths.UnmappedRow())));
  say.Number("the grip of the ground beside the made surface", out.Way.AsideFriction, "x");
  if (!(out.Way.AsideFriction > 0.0)) {
    say.Refuse("the declared templates give unmapped ground no friction, so a wheel that leaves "
               "the made surface would stand on nothing");
    error = "unmapped ground carries no surface friction";
    return false;
  }

  auto &rig = out.State.Rig;
  auto &body = out.State.Body;
  rig = stood.Rig;
  body = outshine::Physics::Rigid();
  body.MassKg = stood.Envelope.MassKg;
  for (int axis = 0; axis < 3; ++axis) { body.InertiaKgM2[axis] = out.Car.InertiaKgM2[axis]; }
  outshine::Placed start;
  if (!corridor.At(0.0, start)) {
    say.Refuse("the corridor does not answer at its own start");
    return false;
  }
  const outshine::Astride under0 =
      outshine::Stand(corridor, start.EastM, start.NorthM, 0.0, 0.0, 50.0);
  const double startAsideM = out.Way.Laid() ? out.Way.At(0.0).AsideM : 0.0;
  say.Number("the fastest the car may move between lane centres",
             out.Way.AsideRatePerM * kMPerKm,
             "mm per metre");
  body.PositionM[0] = start.EastM - std::sin(start.HeadingRad) * startAsideM;
  body.PositionM[1] = under0.HeightM + stood.CentreM[1];
  body.PositionM[2] = -(start.NorthM + std::cos(start.HeadingRad) * startAsideM);
  say.Number("how far from the centreline the car starts, in its own lane", startAsideM, "m");
  {
    const Vec3 up = {{under0.NormalM[0], under0.NormalM[1], -under0.NormalM[2]}};
    say.Number("how far the ground normal leans from vertical where the car stands",
               std::acos(under0.NormalM[1] > 1.0 ? 1.0 : under0.NormalM[1]) * kDegPerHalfTurn /
                   outshine::kPi,
               "deg");
    const Vec3 aheadM = {{std::cos(start.HeadingRad), start.Slope, -std::sin(start.HeadingRad)}};
    outshine::Physics::Lie(body, aheadM, up);
    const Vec3 aheadBody = {{0.0, 0.0, -1.0}};
    Vec3 ahead;
    outshine::Physics::Turn(body.OrientationQ, aheadBody, ahead);
    for (int axis = 0; axis < 3; ++axis) { body.VelocityMs[axis] = kJoinMs * ahead[axis]; }
  }

  out.State.AsideRatePerM = out.Way.AsideRatePerM;

  out.Ready = true;
  return true;
}
} // namespace outshine::Sim
