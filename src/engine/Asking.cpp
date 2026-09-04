#include "Digest.h"
#include "math/Units.h"
#include "math/Vec2.h"
#include "math/Vec3.h"
#include "Log.h"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <expected>
#include <memory>
#include <cmath>
#include <format>
#include <string_view>
#include "Heap.h"
#include "TangentFrame.h"
#include <array>
#include <optional>
#include <span>
#include <numbers>
#include <string>
#include <ratio>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <chrono>
#include <vector>

#include "Fit.h"
#include "ReferenceLine.h"

#include "EngineHeld.h"
#include "GroundMesher.h"

namespace outshine {

namespace {

struct Declaring {
  bool Area = false;
  bool Wet = false;
};

Ground::OsmLayer LayerFor(Declaring what) {
  if (what.Area) {
    return what.Wet ? Ground::OsmLayer::WaterPolygons : Ground::OsmLayer::Buildings;
  }
  return what.Wet ? Ground::OsmLayer::WaterLines : Ground::OsmLayer::Streets;
}

constexpr size_t kBaseSnapshotRows = 40;

constexpr int kZoomMost = 24;

constexpr double kEastStepDeg = 0.0138;

class Instancing final : public Generators::DrawSink {
public:
  explicit Instancing(std::vector<Surrounds::Standing> &into) : Into_(&into) {}

  [[nodiscard]] bool Add(Generators::BodyId body,
                         Generators::ClusterId cluster,
                         const Generators::Scattered &instance) noexcept override {
    if (Full()) { return false; }
    try {
      Into_->push_back(
          {.Body = body.Index(), .Cluster = static_cast<uint32_t>(cluster), .Where = instance});
    } catch (...) { return false; }
    return true;
  }

  [[nodiscard]] bool Full() const noexcept override { return Into_->size() >= kMostInstances; }

private:
  static constexpr size_t kMostInstances = 1u << 20u;
  std::vector<Surrounds::Standing> *Into_;
};

} // namespace

LongitudeLatitude Engine::State::WhereTheEyeStands() const {
  const double anchorLat = Session.Declared.Ground.Origin.LatitudeDeg;
  const double anchorLon = Session.Declared.Ground.Origin.LongitudeDeg;
  LongitudeLatitude stands{.LongitudeDeg = anchorLon, .LatitudeDeg = anchorLat};
  if (Picture.Standing == nullptr || !Picture.Standing->Watched()) { return stands; }
  const TangentFrame anchored =
      TangentFrame::At({.LongitudeDeg = anchorLon, .LatitudeDeg = anchorLat});
  const Vec3 &eye = Picture.Standing->Watching().EyeM;
  Vec3 held;
  for (int axis = 0; axis < 3; ++axis) {
    held[axis] = anchored.OriginEcef()[axis] + eye[0] * anchored.EastEcef()[axis] +
                 eye[1] * anchored.UpEcef()[axis] - eye[2] * anchored.NorthEcef()[axis];
  }
  const Ground::Geo above =
      Ground::EcefToGeoWgs84(Ground::Ecef{.X = held[0], .Y = held[1], .Z = held[2]});
  stands.LatitudeDeg = above.LatitudeDeg;
  stands.LongitudeDeg = above.LongitudeDeg;
  return stands;
}

bool Engine::State::Grows(double atLat, double atLon) {
  Published.Places(
      "generators: bodies already placed", static_cast<double>(World.Placed), "bodies");
  Published.Places(
      "generators: a shipped catalogue stands", World.Shipping.Ready() ? 1.0 : 0.0, "yes/no");
  Published.Places("generators: a ground table stands", World.Table ? 1.0 : 0.0, "yes/no");
  Published.Places(
      "generators: vector data stands", World.Stack.Vectors() != nullptr ? 1.0 : 0.0, "yes/no");
  if (World.Placed > 0 || !World.Shipping.Ready() || !World.Table ||
      World.Stack.Vectors() == nullptr) {
    return false;
  }
  const Generators::Tile region = Generators::Tile::Of(
      World.Stack.Vectors()->Zoom(), {.LongitudeDeg = atLon, .LatitudeDeg = atLat});
  Generators::Fields stands;
  stands.Vectors = World.Stack.Vectors();
  stands.Footprints = &World.Stack.Footprints();
  stands.WaterBodies = &World.Stack.WaterBodies();
  stands.Ways = &World.Stack.Ways();
  Generators::Ground::Snapshot snapshot;
  const Generators::Snapped how = Generators::SnapshotOver(
      region, World.Stack.Ground(), World.Stack.Classes(), stands, World.Table, &snapshot);
  World.Reached = static_cast<int>(kBaseSnapshotRows) + (snapshot.Patch ? 1 : 0) +
                  (snapshot.Classes ? 2 : 0) + (snapshot.Features ? 4 : 0);
  Published.Places("generators: the snapshot",
                   static_cast<double>(static_cast<int>(how)),
                   "0=taken 1=waiting 2=no ground");
  Published.Places("generators: a patch of ground", snapshot.Patch ? 1.0 : 0.0, "yes/no");
  Published.Places("generators: land classes", snapshot.Classes ? 1.0 : 0.0, "yes/no");
  Published.Places("generators: OSM features", snapshot.Features ? 1.0 : 0.0, "yes/no");
  Published.Places(
      "generators: the region it asks about, x", static_cast<double>(region.X()), "tile");
  Published.Places("generators: and y", static_cast<double>(region.Y()), "tile");
  Published.Places("generators: at zoom", static_cast<double>(World.Stack.Vectors()->Zoom()), "z");
  Published.Places("generators: vector tiles that settled",
                   static_cast<double>(World.Stack.Vectors()->Tiles().size()),
                   "tiles");
  Published.Places("generators: vector tiles it refused",
                   static_cast<double>(World.Stack.Vectors()->RefusedTiles()),
                   "tiles");
  Published.Places("generators: that region is settled",
                   World.Stack.Vectors()->Settled(region.X(), region.Y()) ? 1.0 : 0.0,
                   "yes/no");
  World.Grown = how == Generators::Snapped::Taken;
  if (how != Generators::Snapped::Taken) { return false; }
  const int finestZoom = World.Stack.FinestZoomOf(Data::DataKind::VectorMap);
  const Generators::Detail coarseness = Generators::DetailAtRung(finestZoom - region.Zoom());
  Published.Places("generators: rungs coarser than the finest",
                   static_cast<double>(finestZoom - region.Zoom()),
                   "rungs");
  const std::optional<Generators::Ground> over =
      Generators::Ground::Of(region, snapshot, coarseness);
  Published.Places("generators: a ground of that snapshot", over ? 1.0 : 0.0, "yes/no");
  if (!over) { return false; }
  const Generators::RegionPool::Shape shape;
  const Generators::RegionPool::Extent extent{.Reached = over->Where(), .Anywhere = over->Where()};
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
    notes[at].assign(stood.NoteNames().size(), Generators::Yield::Note{});
    yields.emplace_back(lease->Sink(),
                        stood.NoteNames(),
                        std::span<Generators::Yield::Note>(notes[at].data(), notes[at].size()));
  }
  placing.Occupy(*over, std::span<Generators::Yield>(yields.data(), yields.size()));
  for (size_t at = 0; at < yields.size(); ++at) {
    const Generators::Yield &one = yields[at];
    const std::string_view called = placing.At(at).Called();
    World.Placed += one.Placed().Count;
    Published.Places(std::format("generators: {} placed", called),
                     static_cast<double>(one.Placed().Count),
                     "bodies");
    Published.Places(
        std::format("generators: and {} wanted ground another body already held", called),
        static_cast<double>(one.Claims(Generators::Claim::Outcome::Occupied)),
        "claims");
    Published.Places(std::format("generators: and {} wanted ground off the region", called),
                     static_cast<double>(one.Claims(Generators::Claim::Outcome::Outside)),
                     "claims");
  }
  Published.Places("generators: bodies they placed", static_cast<double>(World.Placed), "bodies");
  Published.Places(
      "generators: makers that were asked", static_cast<double>(placing.Count()), "makers");
  if (World.Placed == 0) { return false; }
  Instancing sink(World.Instances);
  World.Shipping.Drawing().Draw(*over,
                                placing,
                                std::span<const Generators::Yield>(yields.data(), yields.size()),
                                lease->Sink().Placed(),
                                sink);
  World.Instanced = World.Instances.size();
  return true;
}

bool Engine::State::Composes() {
  const Heap::Tagged composing("world-compose");
  World.GroundTiles = 0;
  if (!Picture.Standing) {
    Error = "nothing stands to compose a world around";
    return false;
  }
  const Scenario::Document &declared = Session.Declared;
  if (Session.Views && !Session.Views->Active().Sees.Stands.SamplesHeight && !Watches()) {
    return false;
  }
  if (!declared.Ground.Declared) { return true; }
  const double atLat = declared.Ground.Origin.LatitudeDeg;
  const double atLon = declared.Ground.Origin.LongitudeDeg;
  if (!World.Wire) {
    if (Session.Under.Offline) {
      Error = "the ground is FETCHED and the engine was declared offline";
      return false;
    }
    World.Wire = std::make_unique<Fetching>(Fetching::Config{});
  }

  Collecting say;
  if (!World.Stack.Opened() &&
      !World.Stack.Open(Session.Under,
                        {Data::ShippedProviders().begin(), Data::ShippedProviders().end()},
                        {.LongitudeDeg = atLon, .LatitudeDeg = atLat},
                        *World.Wire,
                        say,
                        Session.Declared.Ground.PatienceS)) {
    Error = say.WhyNot();
    return false;
  }

  if (!Session.Declared.Ground.Shape.Kind.empty()) {
    Ground::ShapedGround how;
    how.Kind = Session.Declared.Ground.Shape.Kind;
    how.AmplitudeM = Session.Declared.Ground.Shape.AmplitudeM;
    how.WavelengthM = Session.Declared.Ground.Shape.WavelengthM;
    how.Gradient = Session.Declared.Ground.Shape.Gradient;
    how.BearingDeg = Session.Declared.Ground.Shape.BearingDeg;
    how.FocusLatDeg = atLat;
    how.FocusLonDeg = atLon;
    how.Seed = Session.Declared.Ground.Shape.Seed;
    World.Stack.Pool().Shapes(how);
    Published.Places("ground: a declared relief stands in for the tiles", 1.0, "yes/no");
    const double hereM =
        World.Stack.Ground().At({.LongitudeDeg = atLon, .LatitudeDeg = atLat}).AslM().value_or(0.0);
    const double eastM = World.Stack.Ground()
                             .At({.LongitudeDeg = atLon + kEastStepDeg, .LatitudeDeg = atLat})
                             .AslM()
                             .value_or(0.0);
    Published.Places("ground: the relief says this at the origin", hereM, "m");
    Published.Places("ground: and this a kilometre east", eastM, "m");
  }

  if (!Session.Declared.Ground.Osm.empty()) {
    std::vector<Ground::OsmField::Declared> told;
    told.reserve(Session.Declared.Ground.Osm.size());
    for (const Scenario::Structure &one : Session.Declared.Ground.Osm) {
      Ground::OsmField::Declared made;
      const bool wet = one.Kind == "water";
      const Ground::OsmLayer holds = LayerFor({.Area = one.Area, .Wet = wet});
      made.Layer = OsmLayerName(holds);
      made.Key = "kind";
      made.Value = one.Kind;
      made.WidthM = one.WidthM;
      made.HeightM = one.HeightM;
      made.Area = one.Area;
      made.Bridge = one.Bridge;
      made.Tunnel = one.Tunnel;
      made.Level = one.Level;
      made.LatLon = one.LatLon;
      told.push_back(std::move(made));
    }
    World.Stack.Declares(std::span<const Ground::OsmField::Declared>(told));
    Published.Places(
        "ground: structures a scenario declared", static_cast<double>(told.size()), "structures");
  }

  World.Stack.ShapesFootprintsWith(&World.Shipping.Shaping());
  {
    const double fovDeg =
        Session.Declared.Views.empty() || Session.Declared.Views.front().Sees.FovDeg <= 0.0
            ? 55.0
            : Session.Declared.Views.front().Sees.FovDeg;
    const double highPx = Session.Declared.Render.Frame.HeightPx > 0
                              ? static_cast<double>(Session.Declared.Render.Frame.HeightPx)
                              : 720.0;
    World.Stack.SeeFootprintsWith(highPx /
                                  (2.0 * std::tan(fovDeg * std::numbers::pi / kDegPerTurn)));
    const int vectorZoom = World.Stack.FinestZoomOf(Data::DataKind::VectorMap);
    const double vectorSpanM = 40075017.0 *
                               std::cos(Session.Declared.Ground.Origin.LatitudeDeg * kDeg2Rad) /
                               std::ldexp(1.0, vectorZoom);
    World.Stack.FootprintTilesSpan(vectorSpanM);
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

bool Engine::State::Asks() {
  const Scenario::Document &declared = Session.Declared;
  if (!declared.Ground.Declared) { return true; }
  if (!Picture.Standing || !World.Stack.Opened()) { return true; }
  Around over;
  over.LatitudeDeg = declared.Ground.Origin.LatitudeDeg;
  over.LongitudeDeg = declared.Ground.Origin.LongitudeDeg;
  over.Zoom = World.Stack.FinestZoomOf(Data::DataKind::Elevation);
  over.Asking = true;
  {
    const double tileSpanM =
        40075017.0 * std::cos(over.LatitudeDeg * kDeg2Rad) / std::ldexp(1.0, over.Zoom);
    const double nearest = 4.0 * tileSpanM;
    const double wanted = declared.Ground.SightM > 0.0 ? declared.Ground.SightM : 240000.0;
    over.Levels =
        1 + static_cast<int>(std::ceil(wanted > nearest ? std::log2(wanted / nearest) : 0.0));
  }
  World.Stack.Pool().Focus({.LongitudeDeg = over.LongitudeDeg, .LatitudeDeg = over.LatitudeDeg});
  auto asked = World.Shipping.Covering().Lay(World.Stack.Pool(), over);
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
    Published.Places("mesh jobs the pool finished", static_cast<double>(kept.MeshTiles), "tiles");
    Published.Places("mesh jobs it refused", static_cast<double>(kept.MeshRefused), "tiles");
    Published.Places(
        "mesh jobs with no tile behind them", static_cast<double>(kept.MeshAbsent), "tiles");
    Published.Places("fetches it ran", static_cast<double>(kept.Fetches), "fetches");
    Published.Places("fetches it gave up on", static_cast<double>(kept.FetchGaveUp), "fetches");
    Published.Places("fetches it refused", static_cast<double>(kept.FetchRefused), "fetches");
    Published.Places("jobs it posted", static_cast<double>(kept.Posts), "jobs");
    Published.Places("asks that repeated a posted job", static_cast<double>(kept.Repeats), "asks");
    Published.Places("megabytes it fetched", kept.FetchedMB, "MB");
    Published.Places("jobs still outstanding", static_cast<double>(kept.Outstanding), "jobs");
    Published.Places("keys with jobs parked behind them", static_cast<double>(kept.Parked), "keys");
    Published.Places("jobs parked in all", static_cast<double>(kept.ParkedJobs), "jobs");
    Published.Places("results it holds", static_cast<double>(kept.Held), "results");
    Published.Places(
        "mesh jobs it dropped and will retry", static_cast<double>(kept.MeshDropped), "jobs");
    Published.Places("jobs waiting in the queue", static_cast<double>(kept.QueueDepth), "jobs");
  }
  for (int zoom = 0; zoom < kZoomMost; ++zoom) {
    if (asked->WantedAtZoom[zoom] == 0) { continue; }
    Published.Places("zoom " + std::to_string(zoom) + " wants " +
                         std::to_string(asked->WantedAtZoom[zoom]) + " and still waits for",
                     static_cast<double>(asked->PendingAtZoom[zoom]),
                     "tiles");
  }
  return true;
}

} // namespace outshine
