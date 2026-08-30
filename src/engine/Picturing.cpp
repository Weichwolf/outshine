#include "Log.h"
#include <bit>
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

  [[nodiscard]] bool Add(Generators::BodyId body,
                         Generators::ClusterId cluster,
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

} // namespace

bool Engine::State::Grows(double atLat, double atLon) {
  Published.Places("generators: bodies already placed", (double)World.Placed, "bodies");
  Published.Places(
      "generators: a shipped catalogue stands", World.Shipping.Ready() ? 1.0 : 0.0, "yes/no");
  Published.Places("generators: a ground table stands", World.Table ? 1.0 : 0.0, "yes/no");
  Published.Places(
      "generators: vector data stands", World.Stack.Vectors() != nullptr ? 1.0 : 0.0, "yes/no");
  if (World.Placed > 0 || !World.Shipping.Ready() || !World.Table ||
      World.Stack.Vectors() == nullptr) {
    return false;
  }
  const Generators::Tile region = Generators::Tile::Of(World.Stack.Vectors()->Zoom(), atLat, atLon);
  Generators::Fields stands;
  stands.Vectors = World.Stack.Vectors();
  stands.Footprints = &World.Stack.Footprints();
  stands.WaterBodies = &World.Stack.WaterBodies();
  stands.Ways = &World.Stack.Ways();
  Generators::Ground::Snapshot snapshot;
  const Generators::Snapped how = Generators::SnapshotOver(
      region, World.Stack.Ground(), World.Stack.Classes(), stands, World.Table, &snapshot);
  World.Reached =
      40 + (snapshot.Patch ? 1 : 0) + (snapshot.Classes ? 2 : 0) + (snapshot.Features ? 4 : 0);
  Published.Places("generators: the snapshot", (double)(int)how, "0=taken 1=waiting 2=no ground");
  Published.Places("generators: a patch of ground", snapshot.Patch ? 1.0 : 0.0, "yes/no");
  Published.Places("generators: land classes", snapshot.Classes ? 1.0 : 0.0, "yes/no");
  Published.Places("generators: OSM features", snapshot.Features ? 1.0 : 0.0, "yes/no");
  Published.Places("generators: the region it asks about, x", (double)region.X(), "tile");
  Published.Places("generators: and y", (double)region.Y(), "tile");
  Published.Places("generators: at zoom", (double)World.Stack.Vectors()->Zoom(), "z");
  Published.Places("generators: vector tiles that settled",
                   (double)World.Stack.Vectors()->Tiles().size(),
                   "tiles");
  Published.Places("generators: vector tiles it refused",
                   (double)World.Stack.Vectors()->RefusedTiles(),
                   "tiles");
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
    yields.emplace_back(lease->Sink(),
                        stood.NoteNames(),
                        Span<Generators::Yield::Note>(notes[at].data(), notes[at].size()));
  }
  placing.Occupy(*over, Span<Generators::Yield>(yields.data(), yields.size()));
  for (const Generators::Yield &one : yields) { World.Placed += one.Placed().Count; }
  Published.Places("generators: bodies they placed", (double)World.Placed, "bodies");
  Published.Places("generators: makers that were asked", (double)placing.Count(), "makers");
  if (World.Placed == 0) { return false; }
  Instancing sink(World.Instances);
  World.Shipping.Drawing().Draw(*over,
                                placing,
                                Span<const Generators::Yield>(yields.data(), yields.size()),
                                lease->Sink().Placed(),
                                sink);
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
  if (Session.Views && !Session.Views->Active().Sees.Stands.SamplesHeight && !Watches()) {
    return false;
  }
  if (!declared.Ground.Declared && !overADrive) { return true; }
  const double atLat = overADrive ? way.FrameLat : declared.Ground.Origin.LatitudeDeg;
  const double atLon = overADrive ? way.FrameLon : declared.Ground.Origin.LongitudeDeg;
  if (!World.Wire) {
    if (Session.Under.Offline) {
      Error = "the ground is FETCHED and the engine was declared offline";
      return false;
    }
    World.Wire = std::make_unique<Fetching>(Fetching::Config{});
  }

  Collecting say;
  if (!World.Stack.Opened() &&
      !World.Stack.Open(Session.Under.Cache,
                        Session.Under.Shipped,
                        {Data::ShippedProviders().begin(), Data::ShippedProviders().end()},
                        atLat,
                        atLon,
                        *World.Wire,
                        say,
                        Session.Declared.Ground.PatienceS)) {
    Error = say.WhyNot();
    return false;
  }

  World.Stack.ShapesFootprintsWith(&World.Shaper);
  {
    const double fovDeg =
        Session.Declared.Views.empty() || Session.Declared.Views.front().Sees.FovDeg <= 0.0
            ? 55.0
            : Session.Declared.Views.front().Sees.FovDeg;
    const double highPx = Session.Declared.Render.Frame.HeightPx > 0
                              ? (double)Session.Declared.Render.Frame.HeightPx
                              : 720.0;
    World.Stack.SeeFootprintsWith(highPx / (2.0 * std::tan(fovDeg * std::numbers::pi / 360.0)));
  }
  if (!World.Shipping.Ready() && World.Stack.Vegetated()) {
    std::string why;
    if (!World.Shipping.Stands(
            World.Stack.Vegetation(), std::string(Session.Under.Shipped) + "/world/species", why)) {
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
  over.LatDeg = overADrive ? way.FrameLat : declared.Ground.Origin.LatitudeDeg;
  over.LonDeg = overADrive ? way.FrameLon : declared.Ground.Origin.LongitudeDeg;
  over.Zoom = World.Stack.FinestZoomOf(Data::DataKind::Elevation);
  over.Asking = true;
  {
    const double tileSpanM =
        40075017.0 * std::cos(over.LatDeg * std::numbers::pi / 180.0) / std::ldexp(1.0, over.Zoom);
    const double nearest = 4.0 * tileSpanM;
    const double wanted = declared.Ground.SightM > 0.0 ? declared.Ground.SightM : 240000.0;
    over.Levels = 1 + (int)std::ceil(wanted > nearest ? std::log2(wanted / nearest) : 0.0);
  }
  World.Stack.Pool().Focus(over.LatDeg, over.LonDeg);
  auto asked = LayPatchwork(World.Stack.Pool(), over);
  if (!asked) {
    Error = asked.error();
    return false;
  }
  World.Pending = asked->Pending;
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
                     (double)asked->PendingAtZoom[zoom],
                     "tiles");
  }
  return true;
}

bool Engine::State::Grounds(bool alsoWhenTilesLanded) {
  const Heap::Tagged laying("world-ground");
  auto phaseAt = std::chrono::steady_clock::now();
  auto censusAt = phaseAt;
  auto wiresAt = phaseAt;
  const Scenario &declared = Session.Declared;
  const Sim::Corridor &way = Ticking.Drive.Way;
  const bool overADrive = Ticking.Drove && !way.Fine.empty();
  if (!declared.Ground.Declared && !overADrive) { return true; }
  if (!Picture.Standing || !World.Stack.Opened()) { return true; }
  const double anchorLat = overADrive ? way.FrameLat : declared.Ground.Origin.LatitudeDeg;
  const double anchorLon = overADrive ? way.FrameLon : declared.Ground.Origin.LongitudeDeg;

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
  Published.Places(
      "the ring centres this far from the world's anchor",
      std::hypot((atLat - anchorLat) * 111132.0,
                 (atLon - anchorLon) * 111320.0 * std::cos(anchorLat * std::numbers::pi / 180.0)),
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
    const bool elsewhere = from != World.LaidFrom;
    const bool grew = alsoWhenTilesLanded && resident != World.LaidResident;
    if (World.EverLaid && !elsewhere && !grew) { return true; }
    Published.Places("rebuilds since the world stood", (double)(World.Relaid + 1u), "rebuilds");
    Published.Places("rebuild: the eye walked into another tile", elsewhere ? 1.0 : 0.0, "yes/no");
    Published.Places("rebuild: tiles resident when it did", (double)resident, "tiles");
    Published.Places("rebuild: and resident the time before", (double)World.LaidResident, "tiles");
    World.LaidFrom = from;
    World.LaidResident = resident;
    World.EverLaid = true;
    ++World.Relaid;
  }
  const auto rebuildBegan = std::chrono::steady_clock::now();
  {}

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
  double tallest = -1.0e9, lowest = 1.0e9, tallestOut = 0.0;
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
    if (where.AltM > tallest) {
      tallest = where.AltM;
      tallestOut = std::sqrt(eastM * eastM + northM * northM);
    }
    if (where.AltM < lowest) { lowest = where.AltM; }
  }
  Published.Places("relief: the ring's tallest vertex ABOVE THE ELLIPSOID", tallest, "m");
  Published.Places("relief: and how far out it lies", tallestOut, "m");
  Published.Places("relief: the ring's lowest vertex above the ellipsoid", lowest, "m");
  Published.Places("relief: so the true relief, with the sphere taken out", tallest - lowest, "m");
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
                     leanCount > 0 ? leanSum / (double)leanCount : 0.0,
                     "deg");
  }
  std::vector<float> tinted;
  {
    const std::shared_ptr<const ClassStructure> classes = World.Stack.Classes().Read();
    const Ground::GroundMaterials &wearing = World.Stack.Materials();
    const Render::Medium fallback;
    size_t named = 0;
    if (classes && wearing.Ready()) {
      tinted.resize((inFrame.size() / 3) * 4);
      for (size_t at = 0, one = 0; at + 2 < laid->PositionM.size(); at += 3, ++one) {
        const double held[3] = {laid->OriginEcef[0] + (double)laid->PositionM[at],
                                laid->OriginEcef[1] + (double)laid->PositionM[at + 1],
                                laid->OriginEcef[2] + (double)laid->PositionM[at + 2]};
        const Ground::Geo where = Ground::EcefToGeoWgs84(Ground::Ecef{held[0], held[1], held[2]});
        double edgeM = 0.0;
        int second = -1;
        const int which =
            World.Stack.Classes().ClassAt(*classes, where.LatDeg, where.LonDeg, &edgeM, &second);
        const bool stands = which >= 0 && (size_t)which < wearing.Count();
        if (stands) { ++named; }
        const Ground::GroundMaterials::Material &wore = wearing.At(stands ? (size_t)which : 0);
        tinted[one * 4] = stands ? wore.Albedo[0] : (float)fallback.GroundAlbedo[0];
        tinted[one * 4 + 1] = stands ? wore.Albedo[1] : (float)fallback.GroundAlbedo[1];
        tinted[one * 4 + 2] = stands ? wore.Albedo[2] : (float)fallback.GroundAlbedo[2];
        tinted[one * 4 + 3] = 1.0f;
      }
    }
    if (!tinted.empty()) {
      double wornSum[3] = {0.0, 0.0, 0.0};
      const size_t worn = tinted.size() / 4;
      for (size_t one = 0; one < worn; ++one) {
        for (int channel = 0; channel < 3; ++channel) {
          wornSum[channel] += (double)tinted[one * 4 + (size_t)channel];
        }
      }
      const double wornMean[3] = {
          wornSum[0] / (double)worn, wornSum[1] / (double)worn, wornSum[2] / (double)worn};
      Picture.Standing->Grounding(wornMean);
      Published.Places(
          "lighting: the ground it bounces off, red", 1000.0 * wornMean[0], "albedo/1000");
      Published.Places("lighting: green", 1000.0 * wornMean[1], "albedo/1000");
      Published.Places("lighting: blue", 1000.0 * wornMean[2], "albedo/1000");
    }
    const Render::SubjectEnvironment &lighting = Picture.Standing->AmbientStanding();
    Published.Places("lighting: the sky's own radiance, red", lighting.RadianceLinear[0], "cd/m2");
    Published.Places("lighting: sky green", lighting.RadianceLinear[1], "cd/m2");
    Published.Places("lighting: sky blue", lighting.RadianceLinear[2], "cd/m2");
    Published.Places(
        "lighting: the ground's bounced radiance, red", lighting.GroundLinear[0], "cd/m2");
    Published.Places("lighting: bounce green", lighting.GroundLinear[1], "cd/m2");
    Published.Places("lighting: bounce blue", lighting.GroundLinear[2], "cd/m2");
    Published.Places("the ring's vertices a land class names", (double)named, "vertices");
    Published.Places("out of, for a class", (double)(inFrame.size() / 3), "vertices");
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
      const double held[3] = {
          (double)laid->NormalM[at], (double)laid->NormalM[at + 1], (double)laid->NormalM[at + 2]};
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
      if (!(length > 0.5)) {
        unlengthed += 1.0;
        continue;
      }
      const double upward = y / length;
      if (upward > 0.5) {
        up += 1.0;
      } else if (upward < -0.5) {
        down += 1.0;
      } else {
        sideways += 1.0;
      }
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
      Published.Places("the ground ring's lowest vertex", least, "m");
      Published.Places("the ground ring's highest", most, "m");
    }
  }
  Geometry ground;
  Material bare;
  {
    const Render::Medium held;
    for (int channel = 0; channel < 3; ++channel) {
      bare.BaseColour[channel] = held.GroundAlbedo[channel];
    }
  }
  constexpr double kRoadAboveM = 1.0;
  constexpr double kGapGridM = 20.0;
  constexpr double kDrapeGridM = 32.0;
  if (!tinted.empty()) {
    for (int channel = 0; channel < 3; ++channel) { bare.BaseColour[channel] = 1.0f; }
  }
  const MaterialInstance ringSurface = ground.addSurface("ground", bare);
  const int ringPart = ground.addPart("ground", ringSurface);

  {
    const Ground::BuildingField &prints = World.Stack.Footprints();
    const Raised &built = prints.Built();
    const double *const anchor = prints.Anchor();
    {
      double away = 0.0;
      for (int axis = 0; axis < 3; ++axis) {
        const double step = anchor[axis] - standing.OriginEcef()[axis];
        away += step * step;
      }
      Published.Places(
          "buildings: their anchor lies from the frame's origin", std::sqrt(away), "m");
      Published.Places("buildings: floats in the soup",
                       (double)(built.WallCorners.size() + built.RoofCorners.size()),
                       "floats");
      Published.Places(
          "buildings: the field's last delta began at", (double)prints.AddedFirst(), "floats");
      Published.Places("buildings: and ran for", (double)prints.AddedCount(), "floats");
      Published.Places("buildings: footprints the field holds",
                       (double)prints.Footprints().size(),
                       "footprints");
      if (World.Stack.Vectors() != nullptr) {
        Published.Places("buildings: vector tiles the field settled",
                         (double)World.Stack.Vectors()->Tiles().size(),
                         "tiles");
        Published.Places("buildings: OSM features it holds",
                         (double)World.Stack.Vectors()->Features().size(),
                         "features");
      }
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
        Published.Places(
            "buildings: the ring within 3.2 km runs from", within > 0 ? least : 0.0, "m up");
        Published.Places("buildings: to", within > 0 ? most : 0.0, "m up");
        Published.Places("buildings: over this many ring vertices", (double)within, "vertices");
      }
    }
    if (built.WallRun.size() + built.RoofRun.size() >= 3) {
      std::vector<float> wallPlaces, wallFacing, roofPlaces, roofFacing;
      const auto carry = [&](const std::vector<float> &corners,
                             std::vector<float> &places,
                             std::vector<float> &turned) {
        const size_t count = corners.size() / kTileVertexFloats;
        places.resize(count * 3);
        turned.resize(count * 3);
        for (size_t at = 0; at < count; ++at) {
          const float *const one = corners.data() + at * kTileVertexFloats;
          const double held[3] = {
              anchor[0] + (double)one[0], anchor[1] + (double)one[1], anchor[2] + (double)one[2]};
          double eastM = 0.0, upM = 0.0, northM = 0.0;
          standing.Place(held, &eastM, &upM, &northM);
          places[at * 3] = (float)eastM;
          places[at * 3 + 1] = (float)upM;
          places[at * 3 + 2] = (float)(-northM);
          const double aim[3] = {(double)one[5], (double)one[6], (double)one[7]};
          double alongEast = 0.0, alongUp = 0.0, alongNorth = 0.0;
          standing.Turn(aim, &alongEast, &alongUp, &alongNorth);
          turned[at * 3] = (float)alongEast;
          turned[at * 3 + 1] = (float)alongUp;
          turned[at * 3 + 2] = (float)(-alongNorth);
        }
      };
      carry(built.WallCorners, wallPlaces, wallFacing);
      carry(built.RoofCorners, roofPlaces, roofFacing);
      const std::vector<uint32_t> &wallRun = built.WallRun;
      const std::vector<uint32_t> &roofRun = built.RoofRun;
      const size_t wallVerts = wallPlaces.size() / 3;
      const size_t wallTris = wallRun.size() / 3;
      const size_t vertices = wallVerts + roofPlaces.size() / 3;
      const size_t triangles = wallTris + roofRun.size() / 3;
      const auto placeAt = [&](size_t one) {
        return one < wallVerts ? wallPlaces.data() + one * 3
                               : roofPlaces.data() + (one - wallVerts) * 3;
      };
      const auto turnAt = [&](size_t one) {
        return one < wallVerts ? wallFacing.data() + one * 3
                               : roofFacing.data() + (one - wallVerts) * 3;
      };
      const auto cornerOf = [&](size_t tri, size_t corner) -> size_t {
        return tri < wallTris ? wallRun[tri * 3 + corner]
                              : wallVerts + roofRun[(tri - wallTris) * 3 + corner];
      };
      Material walls;
      walls.BaseColour[0] = 0.74f;
      walls.BaseColour[1] = 0.71f;
      walls.BaseColour[2] = 0.65f;
      walls.Roughness = 0.88f;
      Material tiles;
      tiles.BaseColour[0] = 0.42f;
      tiles.BaseColour[1] = 0.20f;
      tiles.BaseColour[2] = 0.14f;
      tiles.Roughness = 0.72f;
      const MaterialInstance wallSurface = ground.addSurface("walls", walls);
      const MaterialInstance roofSurface = ground.addSurface("roofs", tiles);
      Published.Places(
          "rebuild: the ground ring took",
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt)
              .count(),
          "ms");
      phaseAt = std::chrono::steady_clock::now();
      const int builtPart = ground.addPart("walls", wallSurface);
      const int roofPart = ground.addPart("roofs", roofSurface);
      Published.Places(
          "rebuild: of that, carrying both parts into the frame",
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt)
              .count(),
          "ms");
      censusAt = std::chrono::steady_clock::now();
      if (declared.Render.Audits) {
        std::unordered_map<uint64_t, uint32_t> seenAt;
        std::vector<uint32_t> welded;
        welded.reserve(vertices);
        size_t coincident = 0;
        for (size_t one = 0; one < vertices; ++one) {
          const float *const held = placeAt(one);
          const int64_t cx = (int64_t)std::llround((double)held[0] * 100.0);
          const int64_t cy = (int64_t)std::llround((double)held[1] * 100.0);
          const int64_t cz = (int64_t)std::llround((double)held[2] * 100.0);
          const uint64_t key = (uint64_t)(cx * 73856093LL) ^ (uint64_t)(cy * 19349663LL) ^
                               (uint64_t)(cz * 83492791LL);
          const auto found = seenAt.find(key);
          if (found == seenAt.end()) {
            const uint32_t made = (uint32_t)seenAt.size();
            seenAt.emplace(key, made);
            welded.push_back(made);
          } else {
            ++coincident;
            welded.push_back(found->second);
          }
        }
        std::unordered_map<uint64_t, int> edges;
        size_t degenerate = 0;
        for (size_t tri = 0; tri < triangles; ++tri) {
          const uint32_t corner[3] = {
              welded[cornerOf(tri, 0)], welded[cornerOf(tri, 1)], welded[cornerOf(tri, 2)]};
          if (corner[0] == corner[1] || corner[1] == corner[2] || corner[2] == corner[0]) {
            ++degenerate;
            continue;
          }
          for (int side = 0; side < 3; ++side) {
            const uint32_t from = corner[side];
            const uint32_t to = corner[(side + 1) % 3];
            const uint64_t low = from < to ? from : to;
            const uint64_t high = from < to ? to : from;
            edges[(low << 32) | high] += 1;
          }
        }
        size_t open = 0, overused = 0;
        for (const auto &one : edges) {
          if (one.second == 1) {
            ++open;
          } else if (one.second > 2) {
            ++overused;
          }
        }
        {
          std::unordered_map<uint64_t, uint32_t> whole;
          size_t exact = 0;
          for (size_t one = 0; one < vertices; ++one) {
            uint64_t key = 1469598103934665603ull;
            const float *const held = placeAt(one);
            const float *const aim = turnAt(one);
            for (size_t part = 0; part < 3; ++part) {
              key = (key ^ std::bit_cast<uint32_t>(held[part])) * 1099511628211ull;
            }
            for (size_t part = 0; part < 3; ++part) {
              key = (key ^ std::bit_cast<uint32_t>(aim[part])) * 1099511628211ull;
            }
            if (whole.emplace(key, (uint32_t)whole.size()).second) { continue; }
            ++exact;
          }
          Published.Places(
              "solid: building corners identical in POSITION AND NORMAL", (double)exact, "corners");
          Published.Places(
              "solid: and how many distinct ones remain", (double)whole.size(), "corners");
        }
        Published.Places(
            "solid: building vertices welded away as coincident", (double)coincident, "vertices");
        Published.Places(
            "solid: building vertices standing apart", (double)seenAt.size(), "vertices");
        Published.Places(
            "rebuild: of that, welding and counting edges",
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - censusAt)
                .count(),
            "ms");
        censusAt = std::chrono::steady_clock::now();
        Published.Places("solid: building triangles with two corners in one place",
                         (double)degenerate,
                         "triangles");
        Published.Places("solid: building edges on ONE triangle, so a HOLE", (double)open, "edges");
        Published.Places(
            "solid: building edges on MORE than two, so not a surface", (double)overused, "edges");
        Published.Places("solid: building edges in all", (double)edges.size(), "edges");
      }
      Published.Places("buildings: roof triangles", (double)(roofRun.size() / 3), "triangles");
      Published.Places("buildings: wall triangles", (double)(wallRun.size() / 3), "triangles");
      {
        size_t upright = 0, facingDown = 0;
        for (size_t one = 0; one + 2 < wallFacing.size(); one += 3) {
          const double aloft = (double)wallFacing[one + 1];
          if (aloft < -0.5) {
            ++facingDown;
          } else if (aloft > -0.5 && aloft < 0.5) {
            ++upright;
          }
        }
        Published.Places("buildings: wall normals standing upright", (double)upright, "normals");
        Published.Places("buildings: wall normals facing DOWN", (double)facingDown, "normals");
      }
      const bool tookPlaces =
          builtPart >= 0 && roofPart >= 0 &&
          ground.setPositions(builtPart,
                              std::span<const float>(wallPlaces.data(), wallPlaces.size())) &&
          ground.setPositions(roofPart,
                              std::span<const float>(roofPlaces.data(), roofPlaces.size()));
      const bool tookFacing =
          tookPlaces &&
          ground.setNormals(builtPart,
                            std::span<const float>(wallFacing.data(), wallFacing.size())) &&
          ground.setNormals(roofPart, std::span<const float>(roofFacing.data(), roofFacing.size()));
      const bool tookRun =
          tookFacing &&
          ground.setTriangles(builtPart,
                              std::span<const uint32_t>(wallRun.data(), wallRun.size())) &&
          ground.setTriangles(roofPart, std::span<const uint32_t>(roofRun.data(), roofRun.size()));
      Published.Places("buildings: the part they were given", (double)builtPart, "index");
      Published.Places("buildings: the wall surface", (double)wallSurface.index(), "index");
      Published.Places("buildings: the roof surface", (double)roofSurface.index(), "index");
      Published.Places("buildings: positions taken", tookPlaces ? 1.0 : 0.0, "yes/no");
      Published.Places("buildings: normals taken", tookFacing ? 1.0 : 0.0, "yes/no");
      Published.Places("buildings: triangles taken", tookRun ? 1.0 : 0.0, "yes/no");
      Published.Places("buildings: parts the geometry holds", (double)ground.parts(), "parts");
      Published.Places("building triangles the world meshed", (double)triangles, "triangles");
      Published.Places("buildings: corners the soup holds", (double)vertices, "corners");
      {
        double up = 0.0, down = 0.0, sideways = 0.0, unlengthed = 0.0, inward = 0.0;
        for (size_t at = 0; at < vertices; ++at) {
          const float *const aim = turnAt(at);
          const double x = aim[0], y = aim[1], z = aim[2];
          const double length = std::sqrt(x * x + y * y + z * z);
          if (!(length > 0.5)) {
            unlengthed += 1.0;
            continue;
          }
          const double aloft = y / length;
          if (aloft > 0.5) {
            up += 1.0;
          } else if (aloft < -0.5) {
            down += 1.0;
          } else {
            sideways += 1.0;
          }
        }
        Published.Places("buildings: normals pointing up", up, "normals");
        Published.Places("buildings: normals pointing DOWN", down, "normals");
        Published.Places("buildings: normals lying sideways", sideways, "normals");
        Published.Places("buildings: normals with no length", unlengthed, "normals");
        Published.Places("buildings: normals in all", (double)vertices, "normals");
        (void)inward;
      }
      {
        size_t needles = 0, reaching = 0;
        double longest = 0.0, furthest = 0.0;
        for (size_t tri = 0; tri < triangles; ++tri) {
          const float *const a = placeAt(cornerOf(tri, 0));
          const float *const b = placeAt(cornerOf(tri, 1));
          const float *const c = placeAt(cornerOf(tri, 2));
          const double ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
          const double vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
          const double wx = c[0] - b[0], wy = c[1] - b[1], wz = c[2] - b[2];
          const double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
          const double area = 0.5 * std::sqrt(nx * nx + ny * ny + nz * nz);
          const double edge = std::sqrt(std::max({ux * ux + uy * uy + uz * uz,
                                                  vx * vx + vy * vy + vz * vz,
                                                  wx * wx + wy * wy + wz * wz}));
          if (area < 0.01 && edge > 5.0) {
            ++needles;
            longest = edge > longest ? edge : longest;
          }
          if (edge > 20.0) {
            ++reaching;
            furthest = edge > furthest ? edge : furthest;
          }
        }
        Published.Places("buildings: triangles that are needles", (double)needles, "triangles");
        Published.Places("buildings: the longest edge one carries", longest, "m");
        Published.Places("buildings: triangles reaching over 20 m", (double)reaching, "triangles");
        Published.Places("buildings: the furthest any reaches", furthest, "m");
        Published.Places("buildings: roofs the clipper could not cover",
                         (double)Generators::RoofSurface::UnclippedTaken(),
                         "roofs");
        Published.Places("buildings: roof triangles with a vertex outside their footprint",
                         (double)Generators::RoofSurface::OutsideTaken(),
                         "triangles");
        Published.Places("buildings: seated BELOW the ground they stand on",
                         (double)Generators::BuildingMesh::BuriedTaken(),
                         "buildings");
        Published.Places("buildings: raised with full architecture",
                         (double)Generators::BuildingMesh::RaisedTaken(),
                         "buildings");
        Published.Places("buildings: reduced to a hull box",
                         (double)Generators::BuildingMesh::BoxesTaken(),
                         "buildings");
        Published.Places("buildings: past even a BOX's pixel budget",
                         (double)Generators::BuildingMesh::OverBudgetTaken(),
                         "buildings");
        Published.Places("buildings: meshed with NO pixel scale declared",
                         (double)Generators::BuildingMesh::UnscaledTaken(),
                         "buildings");
        Published.Places("buildings: the farthest one meshed lies",
                         (double)Generators::BuildingMesh::FarthestMTaken(),
                         "m out");
        Published.Places("buildings: and the deepest of them is buried by",
                         (double)Generators::BuildingMesh::DeepestBuriedMmTaken(),
                         "mm");
      }
      {
        double least = 1.0e30, most = -1.0e30, nearest = 1.0e30, farthest = 0.0;
        for (size_t at = 0; at < vertices; ++at) {
          const float *const held = placeAt(at);
          const double up = (double)held[1];
          const double east = (double)held[0], south = (double)held[2];
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

  (void)ground.setPositions(ringPart, std::span<const float>(inFrame.data(), inFrame.size()));
  (void)ground.setNormals(ringPart,
                          std::span<const float>(laid->NormalM.data(), laid->NormalM.size()));
  (void)ground.setTriangles(ringPart,
                            std::span<const uint32_t>(laid->Index.data(), laid->Index.size()));
  if (!tinted.empty()) {
    (void)ground.setColours(ringPart, std::span<const float>(tinted.data(), tinted.size()));
  }

  std::unordered_map<uint64_t, float> drawnGround;
  {
    drawnGround.reserve(inFrame.size() / 3);
    for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
      const int64_t east = (int64_t)std::llround((double)inFrame[at] / kDrapeGridM);
      const int64_t south = (int64_t)std::llround((double)inFrame[at + 2] / kDrapeGridM);
      const uint64_t key = ((uint64_t)(east + 0x20000000) << 32) | (uint64_t)(south + 0x20000000);
      const auto stood = drawnGround.find(key);
      if (stood == drawnGround.end() || inFrame[at + 1] > stood->second) {
        drawnGround[key] = inFrame[at + 1];
      }
    }
  }
  const auto drapedOver = [&drawnGround](double eastM, double southM, double fallback) {
    const int64_t east = (int64_t)std::llround(eastM / kDrapeGridM);
    const int64_t south = (int64_t)std::llround(southM / kDrapeGridM);
    double highest = fallback;
    bool found = false;
    for (int64_t dy = -1; dy <= 1; ++dy) {
      for (int64_t dx = -1; dx <= 1; ++dx) {
        const uint64_t key =
            ((uint64_t)(east + dx + 0x20000000) << 32) | (uint64_t)(south + dy + 0x20000000);
        const auto stood = drawnGround.find(key);
        if (stood == drawnGround.end()) { continue; }
        if (!found || (double)stood->second > highest) { highest = (double)stood->second; }
        found = true;
      }
    }
    return highest;
  };

  {
    Published.Places(
        "rebuild: of that, the census over every triangle",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - censusAt)
            .count(),
        "ms");
    wiresAt = std::chrono::steady_clock::now();
    const Ground::StreetField &ways = World.Stack.Ways();
    const Ground::OsmField *const vectors = World.Stack.Vectors();
    std::vector<float> places, facing;
    std::vector<uint32_t> order;
    size_t laidWays = 0, refusedWays = 0;
    if (vectors != nullptr) {
      const std::span<const double> points = vectors->Points();
      for (const Ground::StreetField::Way &lane : ways.Ways()) {
        if (lane.Form != Ground::StreetField::Shape::Ribbon || lane.PointCount < 2 ||
            !(lane.HalfWidthM > 0.0f)) {
          ++refusedWays;
          continue;
        }
        bool whole = true;
        std::vector<double> left, right;
        left.reserve(lane.PointCount * 3);
        right.reserve(lane.PointCount * 3);
        for (uint32_t step = 0; step < lane.PointCount && whole; ++step) {
          const size_t at = ((size_t)lane.FirstPoint + step) * 2;
          if (at + 1 >= points.size()) {
            whole = false;
            break;
          }
          const double lat = points[at], lon = points[at + 1];
          const uint32_t before = step == 0 ? step : step - 1;
          const uint32_t after = step + 1 < lane.PointCount ? step + 1 : step;
          const size_t from = ((size_t)lane.FirstPoint + before) * 2;
          const size_t to = ((size_t)lane.FirstPoint + after) * 2;
          if (to + 1 >= points.size()) {
            whole = false;
            break;
          }
          const double perLat = 111132.0;
          const double perLon = 111320.0 * std::cos(lat * std::numbers::pi / 180.0);
          double alongE = (points[to + 1] - points[from + 1]) * perLon;
          double alongN = (points[to] - points[from]) * perLat;
          const double run = std::sqrt(alongE * alongE + alongN * alongN);
          if (!(run > 1.0e-6)) {
            whole = false;
            break;
          }
          alongE /= run;
          alongN /= run;
          const double halfM = (double)lane.HalfWidthM;
          const double offLat = -alongE * halfM / perLat, offLon = alongN * halfM / perLon;
          double aslM = 0.0;
          if (!World.Stack.Ground().At(lat, lon).TryAslM(&aslM)) {
            whole = false;
            break;
          }
          left.insert(left.end(), {lat + offLat, lon + offLon, aslM});
          right.insert(right.end(), {lat - offLat, lon - offLon, aslM});
        }
        if (!whole || left.size() < 6) {
          ++refusedWays;
          continue;
        }
        ++laidWays;
        const auto lay = [&](const double *from, double raise) {
          double eastM = 0.0, upM = 0.0, northM = 0.0;
          standing.Place(from[0], from[1], from[2], &eastM, &upM, &northM);
          const double onDrawn = drapedOver(eastM, -northM, upM);
          places.push_back((float)eastM);
          places.push_back((float)(onDrawn + raise));
          places.push_back((float)(-northM));
          facing.push_back(0.0f);
          facing.push_back(1.0f);
          facing.push_back(0.0f);
          order.push_back((uint32_t)(order.size()));
        };
        for (size_t step = 0; step + 1 < left.size() / 3; ++step) {
          const double *const l0 = left.data() + step * 3;
          const double *const r0 = right.data() + step * 3;
          const double *const l1 = left.data() + (step + 1) * 3;
          const double *const r1 = right.data() + (step + 1) * 3;
          lay(l0, kRoadAboveM);
          lay(r1, kRoadAboveM);
          lay(r0, kRoadAboveM);
          lay(l0, kRoadAboveM);
          lay(l1, kRoadAboveM);
          lay(r1, kRoadAboveM);
        }
      }
    }
    {
      std::unordered_map<uint64_t, float> highest;
      highest.reserve(inFrame.size() / 3);
      for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
        const int64_t east = (int64_t)std::llround((double)inFrame[at] / kGapGridM);
        const int64_t south = (int64_t)std::llround((double)inFrame[at + 2] / kGapGridM);
        const uint64_t key = ((uint64_t)(east + 0x20000000) << 32) | (uint64_t)(south + 0x20000000);
        const auto stood = highest.find(key);
        if (stood == highest.end() || inFrame[at + 1] > stood->second) {
          highest[key] = inFrame[at + 1];
        }
      }
      double deepest = 0.0, summed = 0.0;
      size_t compared = 0;
      for (size_t at = 0; at + 2 < places.size(); at += 3) {
        const int64_t east = (int64_t)std::llround((double)places[at] / kGapGridM);
        const int64_t south = (int64_t)std::llround((double)places[at + 2] / kGapGridM);
        const uint64_t key = ((uint64_t)(east + 0x20000000) << 32) | (uint64_t)(south + 0x20000000);
        const auto stood = highest.find(key);
        if (stood == highest.end()) { continue; }
        const double under = (double)stood->second - (double)places[at + 1];
        ++compared;
        summed += under > 0.0 ? under : 0.0;
        deepest = under > deepest ? under : deepest;
      }
      Published.Places("streets: the deepest the ground stands over one", deepest, "m");
      Published.Places(
          "streets: how far on average", compared > 0 ? summed / (double)compared : 0.0, "m");
      Published.Places("streets: vertices compared", (double)compared, "vertices");
    }
    Published.Places("streets: ways laid as ribbons", (double)laidWays, "ways");
    Published.Places("streets: ways it refused", (double)refusedWays, "ways");
    Published.Places("streets: triangles", (double)(order.size() / 3), "triangles");
    if (order.size() >= 3) {
      Material tarmac;
      tarmac.BaseColour[0] = 0.16f;
      tarmac.BaseColour[1] = 0.16f;
      tarmac.BaseColour[2] = 0.17f;
      tarmac.Roughness = 0.92f;
      const MaterialInstance paved = ground.addSurface("streets", tarmac);
      const int pavedPart = ground.addPart("streets", paved);
      const bool tookPaving =
          pavedPart >= 0 &&
          ground.setPositions(pavedPart, std::span<const float>(places.data(), places.size())) &&
          ground.setNormals(pavedPart, std::span<const float>(facing.data(), facing.size())) &&
          ground.setTriangles(pavedPart, std::span<const uint32_t>(order.data(), order.size()));
      Published.Places("streets: the surface they were given", (double)paved.index(), "index");
      Published.Places("streets: the part they were given", (double)pavedPart, "index");
      Published.Places("streets: the geometry took them", tookPaving ? 1.0 : 0.0, "yes/no");
      Published.Places("streets: parts the geometry now holds", (double)ground.parts(), "parts");
    }
  }

  {
    const Ground::WaterField &wet = World.Stack.WaterBodies();
    const Ground::OsmField *const vectors = World.Stack.Vectors();
    std::vector<float> places, facing;
    std::vector<uint32_t> order;
    size_t lidsLaid = 0, lidsRefused = 0;
    if (vectors != nullptr) {
      const std::span<const double> points = vectors->Points();
      for (const Ground::WaterField::Surface &lake : wet.Surfaces()) {
        if (lake.PointCount < 3) {
          ++lidsRefused;
          continue;
        }
        const size_t last = ((size_t)lake.FirstPoint + lake.PointCount) * 2;
        if (last > points.size()) {
          ++lidsRefused;
          continue;
        }
        const size_t began = places.size();
        bool whole = true;
        for (uint32_t step = 1; step + 1 < lake.PointCount && whole; ++step) {
          const uint32_t corners[3] = {0u, step, step + 1u};
          for (const uint32_t corner : corners) {
            const size_t at = ((size_t)lake.FirstPoint + corner) * 2;
            double eastM = 0.0, upM = 0.0, northM = 0.0;
            standing.Place(points[at], points[at + 1], (double)lake.LevelM, &eastM, &upM, &northM);
            places.push_back((float)eastM);
            places.push_back((float)upM);
            places.push_back((float)(-northM));
            facing.push_back(0.0f);
            facing.push_back(1.0f);
            facing.push_back(0.0f);
            order.push_back((uint32_t)(order.size()));
          }
        }
        if (places.size() > began) {
          ++lidsLaid;
        } else {
          ++lidsRefused;
        }
      }
    }
    Published.Places("water: surfaces laid", (double)lidsLaid, "surfaces");
    Published.Places("water: surfaces refused", (double)lidsRefused, "surfaces");
    Published.Places("water: triangles", (double)(order.size() / 3), "triangles");
    if (order.size() >= 3) {
      Material lagoon;
      lagoon.BaseColour[0] = 0.05f;
      lagoon.BaseColour[1] = 0.11f;
      lagoon.BaseColour[2] = 0.16f;
      lagoon.Roughness = 0.14f;
      lagoon.DoubleSided = true;
      const MaterialInstance wetSurface = ground.addSurface("water", lagoon);
      const int wetPart = ground.addPart("water", wetSurface);
      const bool tookWater =
          wetPart >= 0 &&
          ground.setPositions(wetPart, std::span<const float>(places.data(), places.size())) &&
          ground.setNormals(wetPart, std::span<const float>(facing.data(), facing.size())) &&
          ground.setTriangles(wetPart, std::span<const uint32_t>(order.data(), order.size()));
      Published.Places("water: the geometry took it", tookWater ? 1.0 : 0.0, "yes/no");
    }
  }

  Published.Places(
      "rebuild: of that, the streets and the water",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - wiresAt).count(),
      "ms");
  Published.Places(
      "rebuild: and the buildings, streets and water took",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt).count(),
      "ms");
  phaseAt = std::chrono::steady_clock::now();
  const Render::Medium air;
  Material wearing;
  for (int channel = 0; channel < 3; ++channel) {
    wearing.BaseColour[channel] = air.GroundAlbedo[channel];
  }

  const size_t drivenParts = Picture.Standing->CarriedParts();
  Published.Places(
      "restand: the carried count the world hands over", (double)drivenParts, "carried");
  Published.Places("restand: parts in the geometry", (double)ground.parts(), "parts");
  Published.Places(
      "rebuild: and assembling one subject took",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt).count(),
      "ms");
  phaseAt = std::chrono::steady_clock::now();
  if (!Picture.Standing->Restand(std::move(ground), drivenParts, wearing, Error)) { return false; }
  Published.Places(
      "rebuild: of that, walking it into the proxy", Picture.Standing->BuildMs(), "ms");
  Published.Places("rebuild: of THAT, copying the subject", Picture.Standing->CarryMs(), "ms");
  Published.Places(
      "rebuild: standing and submitting INSIDE Build", Picture.Standing->InsideMs(), "ms");
  Published.Places("rebuild: shaping what was built", Picture.Standing->ReshapeMs(), "ms");
  Published.Places("rebuild: composing it", Picture.Standing->ComposeMs(), "ms");
  Published.Places("stand: shaping it a second time", Picture.Standing->ReshapeAgainMs(), "ms");
  Published.Places("stand: the proxy taking it", Picture.Standing->ProxyStandsMs(), "ms");
  Published.Places("stand: placing every part", Picture.Standing->PlacesMs(), "ms");
  Published.Places("stand: dressing them", Picture.Standing->WearsMs(), "ms");
  Published.Places("stand: their emitted radiance", Picture.Standing->LampsMs(), "ms");
  Published.Places("stand: the lamps and the key", Picture.Standing->LitMs(), "ms");
  Published.Places("stand: the medium's own tables", Picture.Standing->MediumMs(), "ms");
  {
    static const char *const kSky[3] = {"the ambient the sky casts, red",
                                        "the ambient the sky casts, green",
                                        "the ambient the sky casts, blue"};
    static const char *const kGround[3] = {"the ambient the ground bounces, red",
                                           "the ambient the ground bounces, green",
                                           "the ambient the ground bounces, blue"};
    for (size_t at = 0; at < 3; ++at) {
      Published.Places(kSky[at], Picture.Standing->AmbientStood()[at], "");
      Published.Places(kGround[at], Picture.Standing->GroundStood()[at], "");
    }
  }
  Published.Places("stand: times the sky was integrated",
                   (double)Picture.Standing->SkyIntegrations(),
                   "integrations");
  Published.Places("stand: sweeping the bounds to frame it", Picture.Standing->FramingMs(), "ms");
  Published.Places("rebuild: resolving its surface", Picture.Standing->ResolveMs(), "ms");
  Published.Places("rebuild: and its bounds", Picture.Standing->BoundsMs(), "ms");
  Published.Places("rebuild: cutting it into clusters", Render::CookedMs(), "ms");
  Published.Places("rebuild: of the streams, packing them", Render::PackedMs(), "ms");
  Published.Places(
      "restand: the geometry handed over, digested", Render::HandedGeometryDigest(), "");
  Published.Places("rebuild: and the device taking them", Render::HandedMs(), "ms");
  Published.Places("rebuild: uploads the residency made",
                   (double)Render::SubjectResidency::UploadsTaken(),
                   "uploads");
  Published.Places(
      "rebuild: megabytes they carried", (double)Render::SubjectResidency::UploadMBTaken(), "MB");
  Published.Places("rebuild: device buffers created",
                   (double)Render::SubjectResidency::BuffersMadeTaken(),
                   "buffers");
  Published.Places("rebuild: staging buffers created",
                   (double)Render::SubjectResidency::StagingMadeTaken(),
                   "buffers");
  Published.Places("rebuild: laying the surface", Picture.Standing->SurfaceMs(), "ms");
  Published.Places("rebuild: settling placements and lights", Picture.Standing->StandMs(), "ms");
  Published.Places("rebuild: and the streams to the device", Picture.Standing->SubmitMs(), "ms");
  Published.Places(
      "rebuild: and handing it to the device took",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt).count(),
      "ms");
  phaseAt = std::chrono::steady_clock::now();
  Published.Places("restand: parts the proxy then stands with",
                   (double)Picture.Standing->PartsStanding(),
                   "parts");
  Published.Places(
      "restand: instances it carries", (double)Picture.Standing->InstancesStanding(), "instances");
  Published.Places(
      "restand: the near plane the renderer stands on", Picture.Standing->NearStanding(), "m");
  for (size_t part = 0; part < Picture.Standing->Shown().Parts.size(); ++part) {
    const Render::ShapePart &one = Picture.Standing->Shown().Parts[part];
    Published.Places("restand: subject part " + std::to_string(part) + " first vertex",
                     (double)one.FirstVertex,
                     "");
    Published.Places("restand: subject part " + std::to_string(part) + " vertex count",
                     (double)one.VertexCount,
                     "");
    Published.Places("restand: subject part " + std::to_string(part) + " first index",
                     (double)one.FirstIndex,
                     "");
    Published.Places("restand: subject part " + std::to_string(part) + " index count",
                     (double)one.IndexCount,
                     "");
  }
  for (size_t part = 0; part < Picture.Standing->PartsStanding(); ++part) {
    const double *const m = Picture.Standing->PlacementStanding(part);
    if (m == nullptr) { continue; }
    double most = 0.0;
    for (int at = 0; at < 16; ++at) { most += std::fabs(m[at]); }
    Published.Places("restand: part " + std::to_string(part) +
                         " placement, sum of the absolute terms",
                     most,
                     "");
    Published.Places(
        "restand: part " + std::to_string(part) + " diagonal", m[0] + m[5] + m[10] + m[15], "");
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
  ++World.Rebuilds;
  World.RebuildMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - rebuildBegan)
          .count();
  Published.Places("and what the last rebuild took", World.RebuildMs, "ms");
  Published.Places("rebuild: times the world was built WHOLE", (double)World.Rebuilds, "rebuilds");
  Published.Places("and how often it was asked about", (double)World.Asked, "walks");
  Published.Places(
      "levels the cascade laid", (double)(over.Zoom - laid->CoarsestZoom + 1), "levels");
  Published.Places("tiles it skipped as already covered", (double)laid->Skipped, "tiles");
  Published.Places("tiles laid bare on the ellipsoid", (double)laid->Bare, "tiles");
  World.Pending = laid->Pending;
  World.Bare = laid->Bare;
  World.Wanted = laid->Tiles;
  Published.Places("tiles that overlap a finer level", (double)laid->Overlapped, "tiles");
  Published.Places("clusters the ring holds", (double)laid->ClustersHeld, "clusters");
  Published.Places("clusters it drew", (double)laid->ClustersDrawn, "clusters");
  Published.Places(
      "cull: clusters carried for the device", (double)laid->Clusters.size(), "clusters");
  Published.Places(
      "cull: the whole index list they cut from", (double)laid->AllIndex.size(), "indices");
  Published.Places(
      "cull: against the list the CPU selected", (double)laid->Index.size(), "indices");
  Published.Places("cull: clusters the ring holds", (double)laid->ClustersHeld, "clusters");
  Published.Places("cull: clusters the CPU drew", (double)laid->ClustersDrawn, "clusters");
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
  if (!Core::Live::Open(
          Picture.Device, std::move(wanted), &Picture.Face, Picture.Standing, Error)) {
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
  World.Blocking =
      TriangleBvh::Over(Span<const float>(corners.data(), corners.size()),
                        Span<const uint32_t>(standing.Indices().data(), standing.Indices().size()));
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
    Published.Places("the step's own time, least", Cost.Advance.LeastMs, "ms");
    Published.Places("the step's own time, most", Cost.Advance.MostMs, "ms");
    Published.Places("steps taken", (double)Cost.Advance.Count, "steps");
  }
  if (Picture.Standing) {
    for (size_t at = 0; at < Render::kStageCount; ++at) {
      const Render::Stage stage = (Render::Stage)at;
      const Render::SceneRenderer::Effort &spent = Picture.Device.Spent(stage);
      if (spent.TookMs <= 0.0 && spent.Draws == 0) { continue; }
      Published.Places(std::string(Row(stage).Name) + ", took", spent.TookMs, "ms");
      Published.Places(std::string(Row(stage).Name) + ", drew", (double)spent.Draws, "draws");
      Published.Places(
          std::string(Row(stage).Name) + ", triangles", (double)spent.Triangles, "triangles");
      Published.Places(
          std::string(Row(stage).Name) + ", surfaces", (double)spent.Surfaces, "slots");
      Published.Places(
          std::string(Row(stage).Name) + ", placements", (double)spent.Placements, "slots");
      Published.Places(
          std::string(Row(stage).Name) + ", textured", (double)spent.Textured, "slots");
      Published.Places(
          std::string(Row(stage).Name) + ", colour images", (double)spent.Palettes, "images");
      Published.Places(
          std::string(Row(stage).Name) + ", device bytes", (double)spent.DeviceBytes, "bytes");
      Published.Places(std::string(Row(stage).Name) + ", placements that differ",
                       (double)spent.Distinct,
                       "rows");
      Published.Places(
          std::string(Row(stage).Name) + ", vertex layouts", (double)spent.Layouts, "layouts");
    }
  }
  if (Cost.Render.Count > 0) {
    Published.Places("the picture's own time, last", Cost.Render.LastMs, "ms");
    Published.Places("the picture's own time, least", Cost.Render.LeastMs, "ms");
    Published.Places("the picture's own time, most", Cost.Render.MostMs, "ms");
    Published.Places("pictures drawn", (double)Cost.Render.Count, "pictures");
  }
  {
    const std::vector<std::string> clashed = Published.Clashed();
    Published.Places("measures published twice in one round", (double)clashed.size(), "rows");
    for (const std::string &one : clashed) {
      Published.Places("published twice in one round: " + one, 1.0, "rows");
    }
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
    const Render::Viewpoint &eye = Picture.Standing->Aimed();
    for (int axis = 0; axis < 3; ++axis) {
      ear.AtM[axis] = eye.EyeM[axis];
      ear.ForwardXyz[axis] = eye.Forward[axis];
      ear.RightXyz[axis] = eye.Right[axis];
    }
  }
  Session.Told.store(next, std::memory_order_release);
}

bool Engine::State::Blocked(const double sourceM[3]) const {
  if (World.Blocking.Empty() || !Picture.Standing) { return false; }
  const Render::Viewpoint &eye = Picture.Standing->Aimed();
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

Result Engine::mix(std::span<float> stereo, int rate) {
  if (!S_->Session.Mixing) {
    if (!S_->Session.Sounding.Stands(
            S_->Session.Declared.Buses, S_->Session.Declared.Sounds, rate, S_->Error)) {
      return std::unexpected(S_->Error);
    }
    S_->Session.Mixing = true;
  }
  const unsigned told = S_->Session.Told.load(std::memory_order_acquire);
  return (S_->Session.Sounding.Fills(
             stereo, S_->Session.Sources[told], S_->Session.Ear[told], S_->Error))
             ? Result{}
             : std::unexpected(S_->Error);
}

bool Engine::render(Extent frame) {
  if (!S_->Stood()) { return false; }
  if (frame.WidthPx > 0 && frame.HeightPx > 0 &&
      (frame.WidthPx != S_->Picture.Frame.WidthPx ||
       frame.HeightPx != S_->Picture.Frame.HeightPx)) {
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
  S_->Published.Places("subject draws", (double)S_->Picture.Device.SubjectDrawCount(), "draws");
  S_->Published.Places(
      "the subject's own animation runs for", S_->Picture.Standing->DurationS(), "s");
  S_->Published.Places(
      "the frames its rate makes of that", (double)S_->Picture.Standing->Frames(), "frames");
  S_->Published.Places("and the instant it is posed at", S_->Picture.Standing->AtS(), "s");
  S_->Published.Places(
      "the pose's own local transforms, digested", S_->Picture.Standing->LocalsDigest(), "");
  S_->Published.Places(
      "the vertices it assembled from them, digested", S_->Picture.Standing->AssembledDigest(), "");
  S_->Published.Places(
      "the geometry the renderer was last offered, digested", Render::HandedGeometryDigest(), "");
  S_->Published.Places("uploads the subject residency has made in all",
                       (double)Render::SubjectResidency::UploadsEver(),
                       "uploads");
  S_->Published.Places("staged crossings the residency flushed",
                       (double)Render::SubjectResidency::CrossingsFlushed(),
                       "crossings");
  S_->Published.Places(
      "subject clusters", (double)S_->Picture.Standing->Shown().Clusters.size(), "clusters");
  {
    const Render::Viewpoint &eye = S_->Picture.Standing->Aimed();
    const double aspect = S_->Picture.Frame.HeightPx > 0 ? (double)S_->Picture.Frame.WidthPx /
                                                               (double)S_->Picture.Frame.HeightPx
                                                         : 1.0;
    const double half = 0.5 * eye.YfovRad;
    const double up = std::tan(half), across = up * aspect;
    size_t kept = 0;
    for (const DagCluster &one : S_->Picture.Standing->Shown().Clusters) {
      const double to[3] = {(double)one.SelfCenter[0] - eye.EyeM[0],
                            (double)one.SelfCenter[1] - eye.EyeM[1],
                            (double)one.SelfCenter[2] - eye.EyeM[2]};
      const double ahead = to[0] * eye.Forward[0] + to[1] * eye.Forward[1] + to[2] * eye.Forward[2];
      const double right = to[0] * eye.Right[0] + to[1] * eye.Right[1] + to[2] * eye.Right[2];
      const double over = to[0] * eye.Up[0] + to[1] * eye.Up[1] + to[2] * eye.Up[2];
      const double radius = (double)one.SelfRadius;
      if (ahead + radius < eye.ZNearM) { continue; }
      if (eye.ZFarM > 0.0 && ahead - radius > eye.ZFarM) { continue; }
      if (std::fabs(right) - radius > across * (ahead > 0.0 ? ahead : 0.0) + radius) { continue; }
      if (std::fabs(over) - radius > up * (ahead > 0.0 ? ahead : 0.0) + radius) { continue; }
      ++kept;
    }
    S_->Published.Places("cull: clusters a frustum would keep", (double)kept, "clusters");
  }
  S_->Published.Places(
      "subject draw calls", (double)S_->Picture.Device.SubjectBatchCount(), "calls");
  S_->Published.Places("plan passes", (double)S_->Picture.Standing->PlanPasses(), "passes");
  for (uint32_t at = 0; at < (uint32_t)Render::kVertexLayouts.size(); ++at) {
    const uint32_t many =
        S_->Picture.Device.SubjectBatchesTaking(static_cast<Render::VertexLayout>(at));
    if (many == 0) { continue; }
    S_->Published.Places("draws taking vertex layout " + std::to_string(at), (double)many, "draws");
  }
  S_->Drew();
  return true;
}

Result Engine::inspect(void) {
  if (!S_->Stood()) { return std::unexpected(S_->Error); }
  if (!S_->Picture.Standing) {
    S_->Error = "nothing stands to be inspected -- a scenario is declared before a frame carries "
                "anything a readback could tell";
    return std::unexpected(S_->Error);
  }
  S_->Inspected();
  return {};
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

bool Engine::readPixels(Buffer which, std::vector<float> &out) {
  if (!S_->Stood()) { return false; }
  if (!S_->Picture.Standing) {
    S_->Error = "nothing stands to be read -- a scenario is declared before a frame carries pixels";
    return false;
  }
  S_->Picture.Device.WantsPixels();
  if (!S_->Picture.Standing->Draw(S_->Error)) { return false; }
  return S_->Picture.Standing->ReadBuffer(which, out, S_->Error);
}

void Engine::logsTo(LogSink *listening) {
  outshine::Log::SetSink(listening);
}

Extent Engine::canvas(void) const {
  return S_->Picture.Frame;
}

bool Engine::camera(Camera &out) const {
  if (!S_->Picture.Standing) { return false; }
  Render::CameraOf(S_->Picture.Standing->Aimed(), out);
  return true;
}

bool Engine::presenting(void) const {
  return S_->Picture.Device.Presents();
}

bool Engine::beginFrame(void) {
  if (!S_->Stood()) { return false; }
  if (!S_->Picture.Standing) {
    S_->Error = "a frame is begun over a scenario, and none stands";
    return false;
  }
  S_->Picture.FrameOpen = true;
  return true;
}

bool Engine::endFrame(void) {
  if (!S_->Picture.FrameOpen) {
    S_->Error = "a frame was ended that was never begun";
    return false;
  }
  S_->Picture.FrameOpen = false;
  if (!S_->Picture.Standing) { return true; }
  return S_->Picture.Standing->Present(S_->Error);
}

bool Engine::flushAndWait(void) {
  if (!S_->Picture.Standing) { return true; }
  return S_->Picture.Standing->Settle(S_->Error);
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

} // namespace outshine
