#include <cmath>
#include "Heap.h"
#include "TangentFrame.h"
#include <unordered_map>
#include <chrono>

#include "EngineHeld.h"

namespace outshine {

namespace {

class Instancing final : public Generators::DrawSink {
public:
  explicit Instancing(std::vector<Surrounds::Standing> &into) : Into_(&into) {}

  [[nodiscard]] bool Add(Generators::BodyId body, Generators::ClusterId cluster,
                         const Generators::Instance &instance) noexcept override {
    if (Full()) { return false; }
    Into_->push_back({body.Index(), (uint32_t)cluster, instance});
    return true;
  }

  [[nodiscard]] bool Full() const noexcept override { return Into_->size() >= kMostInstances; }

private:
  static constexpr size_t kMostInstances = 1u << 20;
  std::vector<Surrounds::Standing> *Into_;
};

}

bool Engine::State::Grows(double atLat, double atLon) {
  // A REFUSAL THAT SAYS NOTHING IS THE HARDEST DEFECT TO FIND. This has four preconditions and used
  // to answer false for any of them without naming which, so `0 placed` at every one of six places
  // was indistinguishable from `nothing grows here`.
  Published.Places("generators: bodies already placed", (double)World.Placed, "bodies");
  Published.Places("generators: a shipped catalogue stands", World.Shipping.Ready() ? 1.0 : 0.0,
                   "yes/no");
  Published.Places("generators: a ground table stands", World.Table ? 1.0 : 0.0, "yes/no");
  Published.Places("generators: vector data stands", World.Stack.Vectors() != nullptr ? 1.0 : 0.0,
                   "yes/no");
  if (World.Placed > 0 || !World.Shipping.Ready() || !World.Table ||
      World.Stack.Vectors() == nullptr) {
    return false;
  }
  const Generators::Tile region =
      Generators::Tile::Of(World.Stack.Vectors()->Zoom(), atLat, atLon);
  Generators::Fields stands;
  stands.Vectors = World.Stack.Vectors();
  stands.Footprints = &World.Stack.Footprints();
  stands.WaterBodies = &World.Stack.WaterBodies();
  stands.Ways = &World.Stack.Ways();
  Generators::Ground::Snapshot snapshot;
  const Generators::Snapped how = Generators::SnapshotOver(
      region, World.Stack.Ground(), World.Stack.Classes(), stands, World.Table, &snapshot);
  World.Reached = 40 + (snapshot.Patch ? 1 : 0) + (snapshot.Classes ? 2 : 0) +
                  (snapshot.Features ? 4 : 0);
  Published.Places("generators: the snapshot", (double)(int)how, "0=taken 1=waiting 2=no ground");
  Published.Places("generators: a patch of ground", snapshot.Patch ? 1.0 : 0.0, "yes/no");
  Published.Places("generators: land classes", snapshot.Classes ? 1.0 : 0.0, "yes/no");
  Published.Places("generators: OSM features", snapshot.Features ? 1.0 : 0.0, "yes/no");
  Published.Places("generators: the region it asks about, x", (double)region.X(), "tile");
  Published.Places("generators: and y", (double)region.Y(), "tile");
  Published.Places("generators: at zoom", (double)World.Stack.Vectors()->Zoom(), "z");
  Published.Places("generators: vector tiles that settled",
                   (double)World.Stack.Vectors()->Tiles().size(), "tiles");
  Published.Places("generators: vector tiles it refused",
                   (double)World.Stack.Vectors()->RefusedTiles(), "tiles");
  Published.Places("generators: that region is settled",
                   World.Stack.Vectors()->Settled((int)region.X(), (int)region.Y()) ? 1.0 : 0.0,
                   "yes/no");
  World.Grown = how == Generators::Snapped::Taken;
  if (how != Generators::Snapped::Taken) { return false; }
  const std::optional<Generators::Ground> over = Generators::Ground::Of(region, snapshot);
  Published.Places("generators: a ground of that snapshot", over ? 1.0 : 0.0, "yes/no");
  if (!over) { return false; }
  Generators::RegionPool::Shape shape;
  Generators::RegionPool::Extent extent{over->Where(), over->Where()};
  Generators::RegionPool pool(extent, shape);
  std::optional<Generators::RegionPool::Lease> lease = pool.TryAcquire(*over);
  Published.Places("generators: a lease on the region", lease ? 1.0 : 0.0, "yes/no");
  if (!lease) { return false; }
  const Generators::GeneratorSet &placing = World.Shipping.Placing();
  std::vector<Generators::Yield> yields;
  std::vector<std::vector<Generators::Yield::Note>> notes(placing.Count());
  yields.reserve(placing.Count());
  for (size_t at = 0; at < placing.Count(); ++at) {
    const Generators::Making &stood = placing.At(at);
    notes[at].assign(stood.NoteNames().Size(), Generators::Yield::Note{});
    yields.emplace_back(lease->Sink(), stood.NoteNames(),
                        Span<Generators::Yield::Note>(notes[at].data(), notes[at].size()));
  }
  placing.Occupy(*over, Span<Generators::Yield>(yields.data(), yields.size()));
  for (const Generators::Yield &one : yields) { World.Placed += one.Placed().Count; }
  Published.Places("generators: bodies they placed", (double)World.Placed, "bodies");
  Published.Places("generators: makers that were asked", (double)placing.Count(), "makers");
  if (World.Placed == 0) { return false; }
  Instancing sink(World.Instances);
  World.Shipping.Drawing().Draw(*over, placing,
                     Span<const Generators::Yield>(yields.data(), yields.size()),
                     lease->Sink().Placed(), sink);
  World.Instanced = World.Instances.size();
  return true;
}

bool Engine::State::Composes(void) {
  const Heap::Tagged composing("world-compose");
  World.GroundTiles = 0;
  if (!Picture.Standing) {
    Error = "nothing stands to compose a world around";
    return false;
  }
  const Scenario &declared = Session.Declared;
  const Sim::Corridor &way = Ticking.Drive.Way;
  const bool overADrive = Ticking.Drove && !way.Fine.empty();
  if (!declared.Ground.Declared && !overADrive) { return true; }
  const double atLat = overADrive ? way.FrameLat : declared.Ground.Lat;
  const double atLon = overADrive ? way.FrameLon : declared.Ground.Lon;
  if (!World.Wire) {
    if (Session.Under.Offline) {
      Error = "the ground is FETCHED and the engine was declared offline";
      return false;
    }
    World.Wire = std::make_unique<Fetching>(Fetching::Config{});
  }

  Collecting say;
  if (!World.Stack.Opened() &&
      !World.Stack.Open(Session.Under.Cache, Session.Under.Shipped,
                      {Data::ShippedProviders().begin(), Data::ShippedProviders().end()},
                      atLat, atLon, *World.Wire, say,
                      Session.Declared.Ground.PatienceS)) {
    Error = say.WhyNot();
    return false;
  }

  World.Stack.ShapesFootprintsWith(&World.Shaper);
  if (!World.Shipping.Ready() && World.Stack.Vegetated()) {
    std::string why;
    if (!World.Shipping.Stands(World.Stack.Vegetation(),
                               std::string(Session.Under.Shipped) + "/world/species", why)) {
      Session.Carried.push_back("nothing shipped stands: " + why);
    }
    World.Table = Generators::TableOf(World.Stack.Vegetation());
  }

  return Grounds(true);
}

bool Engine::State::Asks(void) {
  const Scenario &declared = Session.Declared;
  const Sim::Corridor &way = Ticking.Drive.Way;
  const bool overADrive = Ticking.Drove && !way.Fine.empty();
  if (!declared.Ground.Declared && !overADrive) { return true; }
  if (!Picture.Standing || !World.Stack.Opened()) { return true; }
  Around over;
  over.LatDeg = overADrive ? way.FrameLat : declared.Ground.Lat;
  over.LonDeg = overADrive ? way.FrameLon : declared.Ground.Lon;
  over.Zoom = World.Stack.FinestZoomOf(Data::DataKind::Elevation);
  over.Asking = true;
  {
    const double tileSpanM = 40075017.0 *
                             std::cos(over.LatDeg * std::numbers::pi / 180.0) /
                             std::ldexp(1.0, over.Zoom);
    const double nearest = 4.0 * tileSpanM;
    const double wanted = declared.Ground.SightM > 0.0 ? declared.Ground.SightM : 240000.0;
    over.Levels = 1 + (int)std::ceil(wanted > nearest ? std::log2(wanted / nearest) : 0.0);
  }
  // THE QUEUE PRIORITISES AROUND THE EYE, and until now it prioritised around a point that never
  // moved: `TilePool::Focus` had no caller in the whole tree, so `TileDistance` measured from the
  // pool's construction origin whatever the camera did. Cesium orders its load queue by distance to
  // the camera for the same reason -- what the viewer is standing in front of is what must arrive.
  World.Stack.Pool().Focus(over.LatDeg, over.LonDeg);
  auto asked = LayPatchwork(World.Stack.Pool(), over);
  if (!asked) {
    Error = asked.error();
    return false;
  }
  World.Pending = asked->Pending;
  World.Bare = asked->Bare;
  World.Wanted = asked->Tiles;
  World.AskedPending = asked->Pending;
  World.AskedWanted = asked->Tiles;
  {
    const Ground::TilePool::Ledger kept = World.Stack.Pool().Counters();
    Published.Places("mesh jobs the pool finished", (double)kept.MeshTiles, "tiles");
    Published.Places("mesh jobs it refused", (double)kept.MeshRefused, "tiles");
    Published.Places("mesh jobs with no tile behind them", (double)kept.MeshAbsent, "tiles");
    Published.Places("fetches it ran", (double)kept.Fetches, "fetches");
    Published.Places("fetches it gave up on", (double)kept.FetchGaveUp, "fetches");
    Published.Places("fetches it refused", (double)kept.FetchRefused, "fetches");
    Published.Places("jobs it posted", (double)kept.Posts, "jobs");
    Published.Places("asks that repeated a posted job", (double)kept.Repeats, "asks");
    Published.Places("megabytes it fetched", kept.FetchedMB, "MB");
    Published.Places("jobs still outstanding", (double)kept.Outstanding, "jobs");
    Published.Places("keys with jobs parked behind them", (double)kept.Parked, "keys");
    Published.Places("jobs parked in all", (double)kept.ParkedJobs, "jobs");
    Published.Places("results it holds", (double)kept.Held, "results");
    Published.Places("mesh jobs it dropped and will retry", (double)kept.MeshDropped, "jobs");
    Published.Places("jobs waiting in the queue", (double)kept.QueueDepth, "jobs");
  }
  for (int zoom = 0; zoom < 24; ++zoom) {
    if (asked->WantedAtZoom[zoom] == 0) { continue; }
    Published.Places("zoom " + std::to_string(zoom) + " wants " +
                         std::to_string(asked->WantedAtZoom[zoom]) + " and still waits for",
                     (double)asked->PendingAtZoom[zoom], "tiles");
  }
  return true;
}

bool Engine::State::Grounds(bool alsoWhenTilesLanded) {
  const Heap::Tagged laying("world-ground");
  const Scenario &declared = Session.Declared;
  const Sim::Corridor &way = Ticking.Drive.Way;
  const bool overADrive = Ticking.Drove && !way.Fine.empty();
  if (!declared.Ground.Declared && !overADrive) { return true; }
  if (!Picture.Standing || !World.Stack.Opened()) { return true; }
  const double anchorLat = overADrive ? way.FrameLat : declared.Ground.Lat;
  const double anchorLon = overADrive ? way.FrameLon : declared.Ground.Lon;

  double atLat = anchorLat, atLon = anchorLon;
  if (Picture.Standing->Watched()) {
    const TangentFrame anchored = TangentFrame::At(anchorLat, anchorLon);
    const double *const eye = Picture.Standing->Watching().EyeM;
    double held[3];
    for (int axis = 0; axis < 3; ++axis) {
      held[axis] = anchored.OriginEcef()[axis] + eye[0] * anchored.EastEcef()[axis] +
                   eye[1] * anchored.UpEcef()[axis] - eye[2] * anchored.NorthEcef()[axis];
    }
    const Ground::Geo above = Ground::EcefToGeoWgs84(Ground::Ecef{held[0], held[1], held[2]});
    atLat = above.LatDeg;
    atLon = above.LonDeg;
  }
  Published.Places("the ring centres this far from the world's anchor",
                   std::hypot((atLat - anchorLat) * 111132.0,
                              (atLon - anchorLon) * 111320.0 *
                                  std::cos(anchorLat * std::numbers::pi / 180.0)),
                   "m");

  Around over;
  over.LatDeg = atLat;
  over.LonDeg = atLon;
  over.Zoom = World.Stack.FinestZoomOf(Data::DataKind::Elevation);
  {
    const double tileSpanM =
        40075017.0 * std::cos(atLat * std::numbers::pi / 180.0) / std::ldexp(1.0, over.Zoom);
    const double nearest = 4.0 * tileSpanM;
    const double wanted = declared.Ground.SightM > 0.0 ? declared.Ground.SightM : 240000.0;
    const double doublings = wanted > nearest ? std::log2(wanted / nearest) : 0.0;
    over.Levels = 1 + (int)std::ceil(doublings);
    Published.Places("the sight a scenario declares", wanted, "m");
    Published.Places("and what one tile spans at the finest zoom", tileSpanM, "m");
  }
  if (!Watches()) { return false; }
  if (Picture.Standing->Watched()) {
    const double *const at = Picture.Standing->Watching().EyeM;
    const TangentFrame eyed = TangentFrame::At(atLat, atLon);
    for (int axis = 0; axis < 3; ++axis) {
      over.EyeM[axis] = eyed.OriginEcef()[axis] + at[0] * eyed.EastEcef()[axis] +
                        at[1] * eyed.UpEcef()[axis] - at[2] * eyed.NorthEcef()[axis];
      over.Up[axis] = (float)eyed.UpEcef()[axis];
    }
  }
  if (Picture.Frame.HeightPx > 0) {
    const double halfFov = 0.5 * 55.0 * std::numbers::pi / 180.0;
    over.FocalPx = (float)(0.5 * (double)Picture.Frame.HeightPx / std::tan(halfFov));
  }
  // TWO REASONS TO RE-LAY, AND NO OTHERS: the eye moved into a different tile, so the walk wants a
  // different set; or the last pass was INCOMPLETE and tiles have since landed. Otherwise the
  // terrain that stands is the terrain that was asked for, and rebuilding it costs a full
  // `Gltf::Subject` assemble and a `Restand` for nothing. The old flightbox streamer named the same
  // two reasons and slept between them; CLAUDE.md names it as the rule that work is proportional to
  // what CHANGED. Measured before this guard: `advance` spent every frame inside
  // `Grounds -> Restand -> Live::Stand`.
  {
    const Ground::TileFrac here =
        Ground::ToTileFracClamped(Ground::Geo{.LonDeg = atLon, .LatDeg = atLat}, over.Zoom);
    const uint64_t from = ((uint64_t)(int64_t)std::floor(here.X) << 32) ^
                          (uint64_t)(int64_t)std::floor(here.Y) ^ ((uint64_t)over.Levels << 56);
    World.Stack.Pool().Focus(atLat, atLon);
    ++World.Asked;
    Around asking = over;
    asking.Asking = true;
    auto sees = LayPatchwork(World.Stack.Pool(), asking);
    if (!sees) {
      Error = sees.error();
      return false;
    }
    World.Pending = sees->Pending;
    World.Bare = sees->Bare;
    World.Wanted = sees->Tiles;
    World.AskedPending = sees->Pending;
    World.AskedWanted = sees->Tiles;
    const size_t resident = sees->Tiles > sees->Pending ? sees->Tiles - sees->Pending : 0;
    // BUILDING THE TERRAIN IS A ONE-OFF, AND A STANDING CAMERA HAS NOTHING TO DO. The frame path
    // re-lays for exactly ONE reason: the eye walked into a different tile, so the walk wants a
    // different set. Tiles landing is the OTHER reason to re-lay, and it belongs to whoever is
    // waiting for them -- `preload` while the world comes in, `Composes` at stand-up -- never to
    // `advance`. Without that split, a tile arriving on almost every frame during load made
    // "incomplete" true on every frame, and each one paid a full vertex build, a `Gltf::Subject`
    // assemble and a `Restand`.
    const bool elsewhere = from != World.LaidFrom;
    const bool grew = alsoWhenTilesLanded && resident != World.LaidResident;
    if (World.EverLaid && !elsewhere && !grew) { return true; }
    World.LaidFrom = from;
    World.LaidResident = resident;
    World.EverLaid = true;
    ++World.Relaid;
  }
  const auto rebuildBegan = std::chrono::steady_clock::now();
  {
  }

  auto laid = LayPatchwork(World.Stack.Pool(), over);
  if (!laid) {
    Error = laid.error();
    return false;
  }

  const double frameLat = anchorLat;
  const double frameLon = anchorLon;
  const TangentFrame standing = TangentFrame::At(frameLat, frameLon);
  std::vector<float> inFrame;
  inFrame.resize(laid->PositionM.size());
  double sank = 0.0, sankAt = 0.0;
  for (size_t at = 0; at + 2 < laid->PositionM.size(); at += 3) {
    const double held[3] = {laid->OriginEcef[0] + (double)laid->PositionM[at],
                            laid->OriginEcef[1] + (double)laid->PositionM[at + 1],
                            laid->OriginEcef[2] + (double)laid->PositionM[at + 2]};
    double eastM = 0.0, upM = 0.0, northM = 0.0;
    standing.Place(held, &eastM, &upM, &northM);
    inFrame[at] = (float)eastM;
    inFrame[at + 1] = (float)upM;
    inFrame[at + 2] = (float)(-northM);
    const Ground::Geo where = Ground::EcefToGeoWgs84(Ground::Ecef{held[0], held[1], held[2]});
    const double below = where.AltM - upM;
    if (below > sank) {
      sank = below;
      sankAt = std::sqrt(eastM * eastM + northM * northM);
    }
  }
  {
    std::unordered_map<uint64_t, float> met;
    std::unordered_map<uint64_t, size_t> met2;
    met.reserve(inFrame.size() / 3);
    met2.reserve(inFrame.size() / 3);
    double widest = 0.0, leaning = 0.0, leanSum = 0.0;
    size_t shared = 0, leanCount = 0;
    for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
      const int64_t east = (int64_t)std::llround((double)inFrame[at] * 4.0);
      const int64_t south = (int64_t)std::llround((double)inFrame[at + 2] * 4.0);
      const uint64_t key = ((uint64_t)(east + 0x20000000) << 32) | (uint64_t)(south + 0x20000000);
      const auto stood = met.find(key);
      if (stood == met.end()) {
        met.emplace(key, inFrame[at + 1]);
        met2.emplace(key, at);
        continue;
      }
      ++shared;
      const double apart = std::fabs((double)inFrame[at + 1] - (double)stood->second);
      if (apart > widest) { widest = apart; }
      if (at + 2 < laid->NormalM.size() && stood->second == inFrame[at + 1]) {
        const size_t twin = met2[key];
        double dot = 0.0, one = 0.0, two = 0.0;
        for (size_t axis = 0; axis < 3; ++axis) {
          const double a = (double)laid->NormalM[at + axis];
          const double b = (double)laid->NormalM[twin + axis];
          dot += a * b;
          one += a * a;
          two += b * b;
        }
        if (one > 0.0 && two > 0.0) {
          const double leanDeg =
              std::acos(std::fmin(1.0, std::fmax(-1.0, dot / std::sqrt(one * two)))) * 180.0 /
              std::numbers::pi;
          if (leanDeg > leaning) { leaning = leanDeg; }
          leanSum += leanDeg;
          ++leanCount;
        }
      }
    }
    Published.Places("vertices two tiles put in the same place", (double)shared, "vertices");
    Published.Places("and the widest they disagree on height", widest, "m");
    Published.Places("the widest their NORMALS disagree", leaning, "deg");
    Published.Places("and how far those disagree on average",
                     leanCount > 0 ? leanSum / (double)leanCount : 0.0, "deg");
  }
  Published.Places("the ring's vertex that sinks furthest below its own altitude", sank, "m");
  Published.Places("and how far out it lies", sankAt, "m");
  Published.Places("a sphere would sink it by", sankAt * sankAt / (2.0 * Data::kWgs84A), "m");

  {
    double nearest = 1.0e30, atUp = 0.0, farthest = 0.0, farUp = 0.0;
    for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
      const double east = (double)inFrame[at], south = (double)inFrame[at + 2];
      const double away = east * east + south * south;
      if (away < nearest) {
        nearest = away;
        atUp = (double)inFrame[at + 1];
      }
      if (away > farthest) {
        farthest = away;
        farUp = (double)inFrame[at + 1];
      }
    }
    Published.Places("the ring's nearest vertex to the frame origin", std::sqrt(nearest), "m");
    Published.Places("and its up", atUp, "m");
    Published.Places("its farthest vertex", std::sqrt(farthest), "m");
    Published.Places("and THAT one's up", farUp, "m");
  }
  {
    for (size_t at = 0; at + 2 < laid->Index.size(); at += 3) {
      std::swap(laid->Index[at + 1], laid->Index[at + 2]);
    }
    for (size_t at = 0; at + 2 < laid->NormalM.size(); at += 3) {
      const double held[3] = {(double)laid->NormalM[at], (double)laid->NormalM[at + 1],
                              (double)laid->NormalM[at + 2]};
      double alongEast = 0.0, alongUp = 0.0, alongNorth = 0.0;
      standing.Turn(held, &alongEast, &alongUp, &alongNorth);
      laid->NormalM[at] = (float)alongEast;
      laid->NormalM[at + 1] = (float)alongUp;
      laid->NormalM[at + 2] = (float)(-alongNorth);
    }
  }
  {
    double up = 0.0, down = 0.0, sideways = 0.0, unlengthed = 0.0;
    for (size_t at = 0; at + 2 < laid->NormalM.size(); at += 3) {
      const double x = laid->NormalM[at], y = laid->NormalM[at + 1], z = laid->NormalM[at + 2];
      const double length = std::sqrt(x * x + y * y + z * z);
      if (!(length > 0.5)) { unlengthed += 1.0; continue; }
      const double upward = y / length;
      if (upward > 0.5) { up += 1.0; }
      else if (upward < -0.5) { down += 1.0; }
      else { sideways += 1.0; }
    }
    Published.Places("the ring's normals that point up", up, "normals");
    Published.Places("its normals that point DOWN", down, "normals");
    Published.Places("its normals that lie sideways", sideways, "normals");
    {
      double steepest = 0.0, mean = 0.0, counted = 0.0;
      for (size_t at = 0; at + 2 < laid->NormalM.size(); at += 3) {
        const double x = laid->NormalM[at], y = laid->NormalM[at + 1], z = laid->NormalM[at + 2];
        const double length = std::sqrt(x * x + y * y + z * z);
        if (!(length > 1.0e-6)) { continue; }
        const double leanDeg = std::acos(std::fmin(1.0, y / length)) * 180.0 / std::numbers::pi;
        steepest = leanDeg > steepest ? leanDeg : steepest;
        mean += leanDeg;
        counted += 1.0;
      }
      Published.Places("the steepest the ring's surface leans", steepest, "deg");
      Published.Places("how far it leans on average", counted > 0.0 ? mean / counted : 0.0, "deg");
    }
    Published.Places("its normals with no length at all", unlengthed, "normals");
    Published.Places("its normals in all", (double)(laid->NormalM.size() / 3), "normals");
    {
      double least = 1.0e30, most = -1.0e30;
      const std::vector<float> &held = overADrive ? inFrame : laid->PositionM;
      for (size_t at = 1; at < held.size(); at += 3) {
        const double y = (double)held[at];
        if (y < least) { least = y; }
        if (y > most) { most = y; }
      }
      Published.Places("the ring's lowest vertex", least, "m");
      Published.Places("its highest", most, "m");
    }
  }
  Geometry ground;
  // THE SURFACES ARE DECLARED BEFORE THE PARTS THAT WEAR THEM, and the ground's is first so it
  // stays surface 0. A part naming a surface index that does not exist yet wears whatever lands
  // there later.
  Material bare;
  {
    const Render::Medium held;
    for (int channel = 0; channel < 3; ++channel) {
      bare.BaseColour[channel] = held.GroundAlbedo[channel];
    }
  }
  const int ringSurface = ground.Surface("ground", bare);
  const int ringPart = ground.Part("ground", ringSurface);

  // THE BUILDINGS STAND IN THE SAME GEOMETRY AS THE GROUND, one part beside the ring's. They are
  // STATIC map data, every one with its own footprint, so there is no prototype to instance -- RAGE
  // bakes map geometry and Unreal merges static meshes for exactly this case, and instancing wins
  // only where one shape repeats. `BuildingField` already meshes each footprint into an ECEF soup
  // relative to its own anchor; nothing had ever installed the mesher or read the result.
  {
    const Ground::BuildingField &prints = World.Stack.Footprints();
    const std::vector<float> &soup = prints.Verts();
    const double *const anchor = prints.Anchor();
    {
      double away = 0.0;
      for (int axis = 0; axis < 3; ++axis) {
        const double step = anchor[axis] - standing.OriginEcef()[axis];
        away += step * step;
      }
      Published.Places("buildings: their anchor lies from the frame's origin", std::sqrt(away), "m");
      Published.Places("buildings: floats in the soup", (double)soup.size(), "floats");
      Published.Places("buildings: the field's last delta began at", (double)prints.AddedFirst(),
                       "floats");
      Published.Places("buildings: and ran for", (double)prints.AddedCount(), "floats");
      Published.Places("buildings: footprints the field holds", (double)prints.Footprints().size(),
                       "footprints");
      {
        double least = 1.0e30, most = -1.0e30;
        size_t within = 0;
        for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
          const double east = (double)inFrame[at], south = (double)inFrame[at + 2];
          if (east * east + south * south > 3200.0 * 3200.0) { continue; }
          const double up = (double)inFrame[at + 1];
          least = up < least ? up : least;
          most = up > most ? up : most;
          ++within;
        }
        Published.Places("buildings: the ring within 3.2 km runs from", within > 0 ? least : 0.0,
                         "m up");
        Published.Places("buildings: to", within > 0 ? most : 0.0, "m up");
        Published.Places("buildings: over this many ring vertices", (double)within, "vertices");
      }
    }
    if (soup.size() >= kTileVertexFloats * 3) {
      const size_t vertices = soup.size() / kTileVertexFloats;
      std::vector<float> raised(vertices * 3), facing(vertices * 3);
      std::vector<uint32_t> run(vertices);
      for (size_t at = 0; at < vertices; ++at) {
        const float *const one = soup.data() + at * kTileVertexFloats;
        const double held[3] = {anchor[0] + (double)one[0], anchor[1] + (double)one[1],
                                anchor[2] + (double)one[2]};
        double eastM = 0.0, upM = 0.0, northM = 0.0;
        standing.Place(held, &eastM, &upM, &northM);
        raised[at * 3] = (float)eastM;
        raised[at * 3 + 1] = (float)upM;
        raised[at * 3 + 2] = (float)(-northM);
        const double aim[3] = {(double)one[5], (double)one[6], (double)one[7]};
        double alongEast = 0.0, alongUp = 0.0, alongNorth = 0.0;
        standing.Turn(aim, &alongEast, &alongUp, &alongNorth);
        facing[at * 3] = (float)alongEast;
        facing[at * 3 + 1] = (float)alongUp;
        facing[at * 3 + 2] = (float)(-alongNorth);
        run[at] = (uint32_t)at;
      }
      // THE BUILDINGS KEEP THEIR OWN WINDING. The swap here was copied from the ring, whose indices
      // `LayPatchwork` swaps for this renderer's facing -- but that swap belongs to the TILE mesher's
      // output, and `BuildingMesh` emits its own consistent order from `Site::Tri`, which computes a
      // normal from the same three vertices it pushes. Swapping it turned every closed body inside
      // out: roofs floated and walls went missing, which is what a backfacing solid looks like.
      // Withdrawn -- it was reasoned from the ring rather than measured on a building.
      // BUILDINGS WEAR THEIR OWN SURFACE. `Restand`'s material overload assigns ONE material to
      // every surface, so buildings came out in the ground's exact albedo -- drawn, correctly
      // placed, and indistinguishable from the field they stand in. Lifted 500 m as a control they
      // were unmistakable, with Rothenburg's street plan legible in their shadows on the ground.
      Material walls;
      walls.BaseColour[0] = 0.62f;
      walls.BaseColour[1] = 0.60f;
      walls.BaseColour[2] = 0.56f;
      walls.Roughness = 0.85f;
      const int builtSurface = ground.Surface("walls", walls);
      const int builtPart = ground.Part("buildings", builtSurface);
      // A DISCARDED REFUSAL IS A DEFECT THAT CANNOT BE SEEN. Every one of these returns whether it
      // took the data and every one of them was thrown away with a (void), so a part that was never
      // made and a soup that was never stored looked exactly like geometry standing in the frame.
      const bool tookPlaces =
          builtPart >= 0 &&
          ground.Positions(builtPart, std::span<const float>(raised.data(), raised.size()));
      const bool tookFacing =
          tookPlaces && ground.Normals(builtPart, std::span<const float>(facing.data(), facing.size()));
      const bool tookRun =
          tookFacing && ground.Triangles(builtPart, std::span<const uint32_t>(run.data(), run.size()));
      Published.Places("buildings: the part they were given", (double)builtPart, "index");
      Published.Places("buildings: their surface", (double)builtSurface, "index");
      Published.Places("buildings: positions taken", tookPlaces ? 1.0 : 0.0, "yes/no");
      Published.Places("buildings: normals taken", tookFacing ? 1.0 : 0.0, "yes/no");
      Published.Places("buildings: triangles taken", tookRun ? 1.0 : 0.0, "yes/no");
      Published.Places("buildings: parts the geometry holds", (double)ground.Parts(), "parts");
      Published.Places("building triangles the world meshed", (double)(vertices / 3), "triangles");
      {
        double up = 0.0, down = 0.0, sideways = 0.0, unlengthed = 0.0, inward = 0.0;
        for (size_t at = 0; at + 2 < vertices * 3; at += 3) {
          const double x = facing[at], y = facing[at + 1], z = facing[at + 2];
          const double length = std::sqrt(x * x + y * y + z * z);
          if (!(length > 0.5)) { unlengthed += 1.0; continue; }
          const double aloft = y / length;
          if (aloft > 0.5) { up += 1.0; }
          else if (aloft < -0.5) { down += 1.0; }
          else { sideways += 1.0; }
        }
        // A WALL FACING THE SUN MUST BE LIT. At 60 deg of solar elevation a vertical face turned
        // toward the sun takes cos(60) = 0.5 of the light against a roof's sin(60) = 0.87, so it
        // reads about 57 per cent of the roof. Every wall in the frame is black, which is what a
        // normal pointing INTO the solid does, and this counts them rather than judging by eye.
        Published.Places("buildings: normals pointing up", up, "normals");
        Published.Places("buildings: normals pointing DOWN", down, "normals");
        Published.Places("buildings: normals lying sideways", sideways, "normals");
        Published.Places("buildings: normals with no length", unlengthed, "normals");
        Published.Places("buildings: normals in all", (double)vertices, "normals");
        (void)inward;
      }
      {
        double least = 1.0e30, most = -1.0e30, nearest = 1.0e30, farthest = 0.0;
        for (size_t at = 0; at < vertices; ++at) {
          const double up = (double)raised[at * 3 + 1];
          const double east = (double)raised[at * 3], south = (double)raised[at * 3 + 2];
          const double away = std::sqrt(east * east + south * south);
          least = up < least ? up : least;
          most = up > most ? up : most;
          nearest = away < nearest ? away : nearest;
          farthest = away > farthest ? away : farthest;
        }
        Published.Places("buildings stand between", least, "m up");
        Published.Places("and", most, "m up");
        Published.Places("their nearest vertex lies", nearest, "m out");
        Published.Places("their farthest", farthest, "m out");
      }
    } else {
      Published.Places("building triangles the world meshed", 0.0, "triangles");
    }
  }

  (void)ground.Positions(ringPart, std::span<const float>(inFrame.data(), inFrame.size()));
  (void)ground.Normals(ringPart,
                       std::span<const float>(laid->NormalM.data(), laid->NormalM.size()));
  (void)ground.Triangles(ringPart, std::span<const uint32_t>(laid->Index.data(),
                                                             laid->Index.size()));

  Gltf::Subject laidGround;
  if (!laidGround.Assemble(ground)) {
    Error = laidGround.Error();
    return false;
  }
  const Render::Medium air;
  Material wearing;
  for (int channel = 0; channel < 3; ++channel) {
    wearing.BaseColour[channel] = air.GroundAlbedo[channel];
  }

  const size_t drivenParts = Picture.Standing->Shown().Parts().size();
  Published.Places("restand: the carried count the world hands over", (double)drivenParts,
                   "carried");
  Published.Places("restand: parts in the geometry", (double)ground.Parts(), "parts");
  if (!Picture.Standing->Restand(laidGround, drivenParts, wearing, Error)) { return false; }
  Published.Places("restand: parts the proxy then stands with",
                   (double)Picture.Standing->PartsStanding(), "parts");
  Published.Places("restand: instances it carries",
                   (double)Picture.Standing->InstancesStanding(), "instances");
  Published.Places("restand: the near plane the renderer stands on",
                   Picture.Standing->NearStanding(), "m");
  for (size_t part = 0; part < laidGround.Parts().size(); ++part) {
    const Gltf::Part &one = laidGround.Parts()[part];
    Published.Places("restand: subject part " + std::to_string(part) + " first vertex",
                     (double)one.FirstVertex, "");
    Published.Places("restand: subject part " + std::to_string(part) + " vertex count",
                     (double)one.VertexCount, "");
    Published.Places("restand: subject part " + std::to_string(part) + " first index",
                     (double)one.FirstIndex, "");
    Published.Places("restand: subject part " + std::to_string(part) + " index count",
                     (double)one.IndexCount, "");
  }
  for (size_t part = 0; part < Picture.Standing->PartsStanding(); ++part) {
    const double *const m = Picture.Standing->PlacementStanding(part);
    if (m == nullptr) { continue; }
    double most = 0.0;
    for (int at = 0; at < 16; ++at) { most += std::fabs(m[at]); }
    Published.Places("restand: part " + std::to_string(part) +
                         " placement, sum of the absolute terms", most, "");
    Published.Places("restand: part " + std::to_string(part) + " diagonal",
                     m[0] + m[5] + m[10] + m[15], "");
  }
  World.GroundTiles = laid->Tiles;
  Published.Places("tiles the ring laid", (double)laid->Tiles, "tiles");
  Published.Places("tiles it is still waiting for", (double)laid->Pending, "tiles");
  Published.Places("tiles the stack does not hold", (double)laid->Absent, "tiles");
  Published.Places("tiles it refused", (double)laid->Refused, "tiles");
  {
    double least = 1.0e30, most = -1.0e30;
    for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
      const double up = (double)inFrame[at + 1];
      if (up < least) { least = up; }
      if (up > most) { most = up; }
    }
    double west = 1.0e30, east = -1.0e30, north = 1.0e30, south = -1.0e30;
    for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
      const double alongE = (double)inFrame[at];
      const double alongS = (double)inFrame[at + 2];
      if (alongE < west) { west = alongE; }
      if (alongE > east) { east = alongE; }
      if (alongS < north) { north = alongS; }
      if (alongS > south) { south = alongS; }
    }
    if (most >= least) {
      Published.Places("the ring's lowest vertex", least, "m");
      Published.Places("its highest", most, "m");
      Published.Places("so the relief it carries", most - least, "m");
      Published.Places("and the ground it spans, east to west", east - west, "m");
      Published.Places("north to south", south - north, "m");
      double summed = 0.0;
      size_t counted = 0;
      for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
        summed += (double)inFrame[at + 1];
        ++counted;
      }
      const double mean = counted > 0 ? summed / (double)counted : 0.0;
      size_t adrift = 0;
      for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
        const double off = (double)inFrame[at + 1] - mean;
        if (off > 500.0 || off < -500.0) { ++adrift; }
      }
      Published.Places("the height its vertices average", mean, "m");
      Published.Places("vertices more than 500 m from that average", (double)adrift, "vertices");
      Published.Places("out of", (double)counted, "vertices");
    }
  }
  Published.Places("the sun stands this high", Picture.Standing->Standing().KeyElevationDeg, "deg");
  Published.Places("and bears", Picture.Standing->Standing().KeyBearingDeg, "deg");
  Published.Places("its key light", Picture.Standing->Standing().KeyLux, "lux");
  Published.Places("times the terrain was rebuilt", (double)World.Relaid, "rebuilds");
  World.RebuildMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                              rebuildBegan)
                        .count();
  Published.Places("and what the last rebuild took", World.RebuildMs, "ms");
  Published.Places("and how often it was asked about", (double)World.Asked, "walks");
  Published.Places("levels the cascade laid", (double)(over.Zoom - laid->CoarsestZoom + 1), "levels");
  Published.Places("tiles it skipped as already covered", (double)laid->Skipped, "tiles");
  Published.Places("tiles laid bare on the ellipsoid", (double)laid->Bare, "tiles");
  World.Pending = laid->Pending;
  World.Bare = laid->Bare;
  World.Wanted = laid->Tiles;
  Published.Places("tiles that overlap a finer level", (double)laid->Overlapped, "tiles");
  Published.Places("clusters the ring holds", (double)laid->ClustersHeld, "clusters");
  Published.Places("clusters it drew", (double)laid->ClustersDrawn, "clusters");
  Published.Places("the worst error any of them carries", laid->WorstErrM, "m");
  return true;
}

bool Engine::State::Stood(void) {
  if (Picture.Standing) { return true; }
  if (!Picture.Targeted) {
    Error = "no canvas stands, so there is nowhere to draw -- the client hands one in through "
            "DrawsInto";
    return false;
  }
  Core::Declaration wanted = Picture.Shown;
  wanted.SurfaceWidthPx = Picture.Frame.WidthPx;
  wanted.SurfaceHeightPx = Picture.Frame.HeightPx;
  if (!Core::Live::Open(Picture.Device, std::move(wanted), &Picture.Face, Picture.Standing,
                        Error)) {
    return false;
  }
  if (!Picture.Carrying) { return true; }
  Picture.Carrying = false;
  return Picture.Standing->Restand(Picture.Handed, 0, Error);
}

void Engine::State::Blocks(const Gltf::Subject &standing) {
  const std::vector<double> &positionsM = standing.PositionsM();
  std::vector<float> corners(positionsM.size());
  for (size_t at = 0; at < positionsM.size(); ++at) { corners[at] = (float)positionsM[at]; }
  World.Blocking = TriangleBvh::Over(Span<const float>(corners.data(), corners.size()),
                                     Span<const uint32_t>(standing.Indices().data(),
                                                          standing.Indices().size()));
}

void Engine::State::Tells(void) {
  const Heap::Tagged telling("frame-tells");
  for (size_t at = 0; at < Heap::TagCount(); ++at) {
    const char *const tag = Heap::TagAt(at);
    if (tag == nullptr || Heap::TakenAt(at) == 0) { continue; }
    Published.Places(std::string("heap taken under ") + tag, (double)Heap::TakenAt(at), "bytes");
  }
  if (Cost.Advance.Count > 0) {
    Published.Places("the step's own time, last", Cost.Advance.LastMs, "ms");
    Published.Places("its least", Cost.Advance.LeastMs, "ms");
    Published.Places("its most", Cost.Advance.MostMs, "ms");
    Published.Places("steps taken", (double)Cost.Advance.Count, "steps");
  }
  if (Picture.Standing) {
    for (size_t at = 0; at < Render::kStageCount; ++at) {
      const Render::Stage stage = (Render::Stage)at;
      const Render::Renderer::Effort &spent = Picture.Device.Spent(stage);
      if (spent.TookMs <= 0.0 && spent.Draws == 0) { continue; }
      Published.Places(std::string(Row(stage).Name) + ", took", spent.TookMs, "ms");
      Published.Places(std::string(Row(stage).Name) + ", drew", (double)spent.Draws, "draws");
      Published.Places(std::string(Row(stage).Name) + ", triangles", (double)spent.Triangles,
                       "triangles");
      Published.Places(std::string(Row(stage).Name) + ", surfaces", (double)spent.Surfaces,
                       "slots");
      Published.Places(std::string(Row(stage).Name) + ", placements", (double)spent.Placements,
                       "slots");
      Published.Places(std::string(Row(stage).Name) + ", textured", (double)spent.Textured,
                       "slots");
      Published.Places(std::string(Row(stage).Name) + ", colour images", (double)spent.Palettes,
                       "images");
      Published.Places(std::string(Row(stage).Name) + ", device bytes",
                       (double)spent.DeviceBytes, "bytes");
      Published.Places(std::string(Row(stage).Name) + ", placements that differ",
                       (double)spent.Distinct, "rows");
      Published.Places(std::string(Row(stage).Name) + ", vertex layouts", (double)spent.Layouts,
                       "layouts");
    }
  }
  if (Cost.Render.Count > 0) {
    Published.Places("the picture's own time, last", Cost.Render.LastMs, "ms");
    Published.Places("its least", Cost.Render.LeastMs, "ms");
    Published.Places("its most", Cost.Render.MostMs, "ms");
    Published.Places("pictures drawn", (double)Cost.Render.Count, "pictures");
  }

  const unsigned next = (Session.Told.load(std::memory_order_relaxed) + 1u) & 1u;
  std::vector<Audio::Heard> &sources = Session.Sources[next];
  sources.clear();
  sources.reserve(Session.Declared.Sounds.size());
  for (const Sound &declared : Session.Declared.Sounds) {
    Audio::Heard where;
    where.Id = declared.Id;
    if (declared.On.empty()) {
      where.Standing = !declared.Heard.Positional;
      sources.push_back(where);
      continue;
    }
    const Physics::Rigid *stood = nullptr;
    if (Ticking.Drove && Session.Declared.Bodies.size() == 1) {
      stood = &Ticking.Drive.State.Body;
    } else if (!Ticking.Freestanding.empty()) {
      stood = &Ticking.Freestanding.front();
    }
    if (stood != nullptr) {
      where.Standing = true;
      for (int axis = 0; axis < 3; ++axis) {
        where.AtM[axis] = stood->PositionM[axis];
        where.VelocityMs[axis] = stood->VelocityMs[axis];
      }
      where.Blocked = Blocked(where.AtM) ? 1.0 : 0.0;
    }
    sources.push_back(where);
  }

  Audio::Listening &ear = Session.Ear[next];
  ear = Audio::Listening{};
  if (Picture.Standing) {
    const Gltf::Viewpoint &eye = Picture.Standing->Aimed();
    for (int axis = 0; axis < 3; ++axis) {
      ear.AtM[axis] = eye.EyeM[axis];
      ear.ForwardXyz[axis] = eye.Forward[axis];
      ear.RightXyz[axis] = eye.Right[axis];
    }
  }  Session.Told.store(next, std::memory_order_release);
}

bool Engine::State::Blocked(const double sourceM[3]) const {
  if (World.Blocking.Empty() || !Picture.Standing) { return false; }
  const Gltf::Viewpoint &eye = Picture.Standing->Aimed();
  float fromM[3], along[3];
  double awayM = 0.0;
  for (int axis = 0; axis < 3; ++axis) {
    const double step = sourceM[axis] - eye.EyeM[axis];
    awayM += step * step;
  }
  awayM = std::sqrt(awayM);
  if (!(awayM > 0.0)) { return false; }
  for (int axis = 0; axis < 3; ++axis) {
    fromM[axis] = (float)eye.EyeM[axis];
    along[axis] = (float)((sourceM[axis] - eye.EyeM[axis]) / awayM);
  }
  return World.Blocking.Occludes(fromM, along, 0.01f, (float)awayM);
}

bool Engine::mix(std::span<float> stereo, int rate) {
  if (!S_->Session.Mixing) {
    if (!S_->Session.Sounding.Stands(S_->Session.Declared.Buses, S_->Session.Declared.Sounds,
                                     rate, S_->Error)) {
      return false;
    }
    S_->Session.Mixing = true;
    S_->Tells();
  }
  const unsigned told = S_->Session.Told.load(std::memory_order_acquire);
  return S_->Session.Sounding.Fills(stereo, S_->Session.Sources[told],
                                    S_->Session.Ear[told], S_->Error);

}

bool Engine::render(Extent frame) {
  if (!S_->Stood()) { return false; }
  if (frame.WidthPx > 0 && frame.HeightPx > 0 &&
      (frame.WidthPx != S_->Picture.Frame.WidthPx || frame.HeightPx != S_->Picture.Frame.HeightPx)) {
    S_->Error = "this engine stands on a " + std::to_string(S_->Picture.Frame.WidthPx) + "x" +
                std::to_string(S_->Picture.Frame.HeightPx) + " canvas and was asked to draw " +
                std::to_string(frame.WidthPx) + "x" + std::to_string(frame.HeightPx) +
                " -- a canvas is declared before a scenario stands on it";
    return false;
  }
  const auto began = std::chrono::steady_clock::now();
  if (!S_->Picture.Standing->Draw(S_->Error)) { return false; }
  S_->Cost.Render.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
  S_->Drew();
  return true;
}

bool Engine::inspect(void) {
  if (!S_->Stood()) { return false; }
  if (!S_->Picture.Standing) {
    S_->Error = "nothing stands to be inspected -- a scenario is declared before a frame carries "
                "anything a readback could tell";
    return false;
  }
  S_->Inspected();
  return true;
}

bool Engine::readPixels(std::vector<uint8_t> &rgba) {
  if (!S_->Stood()) { return false; }
  if (!S_->Picture.Standing) {
    S_->Error = "nothing stands to be read -- a scenario is declared before a frame carries pixels";
    return false;
  }
  S_->Picture.Device.WantsPixels();
  if (!S_->Picture.Standing->Draw(S_->Error)) { return false; }
  return S_->Picture.Standing->ReadPixels(rgba, S_->Error);
}

bool Engine::saveScreenshot(std::string_view path) {
  if (!S_->Stood()) { return false; }
  if (!S_->Picture.Standing) {
    S_->Error = "nothing stands to be captured -- a scenario is declared before a frame is kept";
    return false;
  }
  S_->Picture.Device.WantsPixels();
  if (!S_->Picture.Standing->Draw(S_->Error)) { return false; }
  return S_->Picture.Standing->Screenshot(std::string(path), S_->Error);
}

}
