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
constexpr double kPatienceS = 900.0;
constexpr double kJoinMs = 20.0;
}

bool AssembleDrive(const Store &scene, const Assembled &cast, const Column<Vehicle> &vehicles,
                   const Column<Drive> &driven, const WorldSettings &world,
                   Ground::GroundStack &stack, Data::Transport &wire, const Provision &kept,
                   Sink &say, DriveProduct &out) {
  if (kept.CacheDir.empty() || kept.AssetsDir.empty()) {
    say.Refuse("no cache directory or assets root was provisioned");
    return false;
  }
  const outshine::Vehicle *car = vehicles.Get(cast.PlayerBody);
  if (car == nullptr) {
    say.Refuse("the assembled body carries no vehicle declaration");
    return false;
  }
  const outshine::Drive *driveTo = driven.Get(cast.Assignment);
  const bool assigned =
      driveTo != nullptr && scene.TargetOf(cast.PlayerMind, Relation::Assigned) == cast.Assignment;
  if (!assigned) { say.Refuse("the mind carries no assignment"); }
  if (!assigned) { return false; }
  const double fromLatDeg = driveTo->FromLatDeg;
  const double fromLonDeg = driveTo->FromLonDeg;
  const double toLatDeg = driveTo->ToLatDeg;
  const double toLonDeg = driveTo->ToLonDeg;
  out.Car = *car;

  auto &corridor = out.Way.Line;
  auto &stood = out.Stood;

  const double straightM = ApartM(fromLatDeg, fromLonDeg, toLatDeg, toLonDeg, world.RadiusM);
  const double middleLat = 0.5 * (fromLatDeg + toLatDeg);
  const double middleLon = 0.5 * (fromLonDeg + toLonDeg);
  say.Number("start to destination as the crow flies", straightM / 1000.0, "km");
  std::string error;
  out.State.CarWidthM = out.Car.WidthM;
  const double carWidthM = out.State.CarWidthM;
  say.Number("the width the declaration gives the car", carWidthM, "m");
  if (!(carWidthM > 0.0)) {
    say.Refuse("the vehicle declares no width");
    return false;
  }

  if (!stack.Open(kept.CacheDir, kept.AssetsDir, kept.Providers, middleLat, middleLon, wire,
                  say)) {
    return false;
  }

  const int kZoom = stack.FinestZoomOf(Data::DataKind::VectorMap);
  if (kZoom <= 0) {
    say.Refuse("no declared source carries a vector map, so there is no zoom to read the ways "
               "at");
    return false;
  }
  say.Number("the zoom the ways are read at", (double)kZoom, "");

  const double tileGroundM =
      outshine::Ground::kMercatorGirthM * std::cos(middleLat * outshine::kPi / 180.0) /
      (double)(1L << kZoom);
  const int kCorridorRing = 2;
  const long steps = (long)std::ceil(straightM / tileGroundM) + 1;
  const long square = (long)(2 * std::ceil(0.5 * straightM / tileGroundM) + 3);
  say.Number("a tile's ground size at this zoom and latitude", tileGroundM / 1000.0, "km");
  say.Number("stations along the line the corridor is fetched at", (double)steps, "stations");
  say.Number("the ring fetched around each", (double)(2 * kCorridorRing + 1), "tiles across");
  say.Number("what a square covering both cities would have cost", (double)(square * square),
             "tiles");

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
  out.Found.RanOutOfPatience = ranOut;
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
  out.Found.FetchedS = fetchedS;
  say.Number("seconds spent fetching and decoding", fetchedS, "s");

  out.Found.Features = (long)field.Features().size();
  if (field.Features().empty()) {
    say.Refuse("the fetched tiles decode to no feature");
    return false;
  }

  outshine::Ground::GroundMaterials materials;
  if (!materials.Load((kept.AssetsDir + "/world/ground-materials.json").c_str())) {
    say.Refuse("the declared ground materials do not load");
    return false;
  }
  VegetationTemplates widths;
  if (!widths.Load((kept.AssetsDir + "/world/vegetation.json").c_str(), materials)) {
    say.Refuse("the declared width table does not load");
    return false;
  }

  const double quantumM = outshine::Ground::kMercatorGirthM / ((double)(1L << kZoom) * 4096.0);
  Network roads(1.05 * quantumM, world.RadiusM);
  say.Number("the tile's own coordinate quantisation", quantumM, "m");
  const Reaped reaped = Reap(field, widths, carWidthM, roads);
  out.Found.StreetsAbsent = reaped.StreetsAbsent;
  say.Number("the snapping distance just above it", roads.SnapM(), "m");
  say.Number("ways a car can fit down", (double)reaped.Ways, "ways");
  say.Number("points in them", (double)reaped.Points, "points");
  say.Number("ways too narrow for it", (double)reaped.TooNarrow, "ways");
  say.Number("the widest way it refused", reaped.WidestRefusedM, "m");
  say.Number("the narrowest it took", reaped.NarrowestTakenM, "m");
  say.Number("ways whose kind carries no declared width", (double)reaped.Unclassed, "ways");

  say.Number("ways that are not a carriageway at all", (double)reaped.NotACarriageway, "ways");
  say.Number("ways the data marks as a bridge", (double)reaped.Bridges, "ways");
  say.Number("ways it marks as a tunnel", (double)reaped.Tunnels, "ways");
  say.Number("ways that declare a stacking layer", (double)reaped.Layered, "ways");
  say.Number("the deepest layer any of them names", reaped.DeepestLayer, "");
  say.Number("the highest", reaped.HighestLayer, "");
  say.Number("distinct tag keys the vector tiles carry", (double)field.KeyCount(), "keys");
  for (size_t at = 0; at < field.KeyCount(); ++at) {
    say.Say(Line("KEY %s", std::string(field.KeyAt(at)).c_str()));
  }
  if (!reaped.NotCarriageways.empty()) {
    say.Say(Line("NOT CARRIAGEWAYS %s", reaped.NotCarriageways.c_str()));
  }
  out.Found.NotACarriageway = (long)reaped.NotACarriageway;
  say.Number("ways whose kind declares no maximum grade", (double)reaped.Ungraded, "ways");

  if (!reaped.WithoutGrade.empty()) {
    say.Say(Line("UNGRADED KINDS %s", reaped.WithoutGrade.c_str()));
  }
  out.Found.Ways = (long)reaped.Ways;
  out.Found.TooNarrow = (long)reaped.TooNarrow;
  out.Found.Ungraded = (long)reaped.Ungraded;
  out.Found.WidestRefusedM = reaped.WidestRefusedM;

  if (!roads.Weave(error)) {
    say.Refuse(Line("the ways do not weave into a network: %s", error.c_str()));
    return false;
  }
  out.Found.Nodes = (long)roads.NodeCount();
  out.Found.Junctions = (long)roads.JunctionCount();
  say.Number("nodes after snapping", (double)out.Found.Nodes, "nodes");
  say.Number("junctions among them", (double)out.Found.Junctions, "nodes");
  say.Number("edges", (double)roads.EdgeCount(), "edges");
  say.Number("loose ends tied onto an edge they end on", (double)roads.TiedToEdges(), "ends");
  say.Number("cells the tie index holds", (double)roads.CellsInTheTieIndex(), "cells");
  {
    const Network::Pieces broken = roads.InPieces();
    say.Number("pieces the graph falls into", (double)broken.Count, "pieces");
    say.Number("nodes in the largest", (double)broken.Largest, "nodes");
    say.Number("pieces holding fewer than four nodes", (double)broken.UnderFour, "pieces");
    say.Number("nodes stranded in those", (double)broken.InUnderFour, "nodes");
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
    say.Number("places two ways cross in plan without sharing a node", (double)found, "places");
      out.Found.Crossings = (long)found;
    say.Number("segment pairs the crossing sweep tested", (double)swept.PairsTested, "pairs");
    say.Number("segments in the cell that held the most", (double)swept.FullestCell, "segments");
  }

  size_t atFrom = 0, atTo = 0;
  double fromAwayM = 0.0, toAwayM = 0.0;
  if (!roads.Nearest(Waypoint{fromLatDeg, fromLonDeg}, atFrom, fromAwayM) ||
      !roads.Nearest(Waypoint{toLatDeg, toLonDeg}, atTo, toAwayM)) {
    say.Refuse("a waypoint resolves to no node of the network");
    return false;
  }
  out.Found.FromAwayM = fromAwayM;
  out.Found.ToAwayM = toAwayM;
  say.Number("how far the start is from the nearest road node", fromAwayM, "m");
  say.Number("how far the destination is from the nearest road node", toAwayM, "m");
  say.Number("how far each walk is as a share of the drive",
       (fromAwayM + toAwayM) / straightM, "of it");

  stood = outshine::Sim::Stand(out.Car, world.GravityMs2, world.AirDensityKgM3);
  if (!stood.Stood) { say.Refuse(Line("%s", stood.Error.c_str())); }
  if (!stood.Stood) { return false; }
  const double tightestM = stood.TightestM;
  say.Number("the tightest centreline circle the declaration implies", tightestM, "m");
  const Route route =
      roads.Plan(Waypoint{fromLatDeg, fromLonDeg}, Waypoint{toLatDeg, toLonDeg}, tightestM);
  say.Number("turns the search refused as too sharp for the car", (double)route.TurnsRefused, "turns");
  if (!route.Found) { say.Refuse(Line("%s", route.Error.c_str())); }
  say.Number("nodes the search settled", (double)route.Reached, "nodes");
  if (!route.Found) { return false; }
  out.Found.TurnsRefused = (long)route.TurnsRefused;

  say.Number("how far the route runs", route.LengthM / 1000.0, "km");
  say.Number("how far the crow flies", route.StraightM / 1000.0, "km");
  say.Number("the detour that is", route.LengthM / route.StraightM, "x");
  out.Found.RouteLengthM = route.LengthM;
  out.Found.StraightM = route.StraightM;
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
  if (!corridor.At(0.0, start)) {
    say.Refuse("the corridor does not answer at its own start");
    return false;
  }
  const outshine::Standing under0 =
      outshine::Stand(corridor, start.EastM, start.NorthM, 0.0, 0.0, 50.0);
  const double startAsideM = out.Way.Laid() ? out.Way.At(0.0).AsideM : 0.0;
  say.Number("the fastest the car may move between lane centres",
       out.Way.AsideRatePerM * 1000.0,
       "mm per metre");
  body.PositionM[0] = start.EastM - std::sin(start.HeadingRad) * startAsideM;
  body.PositionM[1] = under0.HeightM + stood.CentreM[1];
  body.PositionM[2] = -(start.NorthM + std::cos(start.HeadingRad) * startAsideM);
  say.Number("how far from the centreline the car starts, in its own lane", startAsideM, "m");
  {
    const double up[3] = {under0.NormalM[0], under0.NormalM[1], -under0.NormalM[2]};
    say.Number("how far the ground normal leans from vertical where the car stands",
               std::acos(under0.NormalM[1] > 1.0 ? 1.0 : under0.NormalM[1]) * 180.0 /
                   outshine::kPi,
               "deg");
    const double aheadM[3] = {std::cos(start.HeadingRad), start.Slope,
                              -std::sin(start.HeadingRad)};
    outshine::Physics::Lie(body, aheadM, up);
    const double aheadBody[3] = {0.0, 0.0, -1.0};
    double ahead[3];
    outshine::Physics::Turn(body.OrientationQ, aheadBody, ahead);
    for (int axis = 0; axis < 3; ++axis) { body.VelocityMs[axis] = kJoinMs * ahead[axis]; }
  }

  out.State.AsideRatePerM = out.Way.AsideRatePerM;
  
  out.Ready = true;
  return true;
}
}
