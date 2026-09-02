#include "Digest.h"
#include "Units.h"
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
#include <algorithm>
#include <cmath>
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
#include "GroundYield.h"

namespace outshine {

constexpr double kNoLeastYet = 1.0e9;

constexpr uint32_t kNoVertex = 0xffffffffu;

static_assert(Ground::kStreamGrid == 2 * (kPatchGrid - 1),
              "the elevation stream is opened ONE zoom below the finest tile so its posting equals "
              "the patchwork's vertex spacing; that holds only while a stream tile carries twice "
              "the intervals a patchwork tile lays, and if either grid moves the zoom must be "
              "re-derived rather than kept");

namespace {

constexpr size_t kBaseSnapshotRows = 40;
constexpr double kBroadQuantile = 0.95;
constexpr double kLeastSineBetween = 1.0e-3;
constexpr double kLeastCapM = 0.01;
constexpr double kLeastSpanM = 0.05;
constexpr float kUnlitTint = 0.65f;
constexpr double kAdriftMostM = 500.0;
constexpr float kNearestOccluderM = 0.01f;

constexpr float kLagoonRed = 0.05f;
constexpr float kLagoonGreen = 0.11f;
constexpr float kLagoonBlue = 0.16f;
constexpr float kLagoonRoughness = 0.14f;

constexpr double kFinestCellM = 32.0;
constexpr double kCellPerRung = 8.0;
constexpr int kZoomMost = 24;
constexpr double kEastStepDeg = 0.0138;
constexpr double kPerMille = 1000.0;
constexpr double kFootprintReachM = 3200.0;
constexpr double kSliverAreaM2 = 0.01;
constexpr double kSliverEdgeM = 5.0;
constexpr double kLongEdgeM = 20.0;
constexpr double kUnraisedDeckM = -1.0e29;
constexpr double kRoseLeast = 0.05;

constexpr float kWallRed = 0.74f;
constexpr float kWallGreen = 0.71f;
constexpr float kWallBlue = 0.65f;
constexpr float kWallRoughness = 0.88f;
constexpr float kTileRed = 0.42f;
constexpr float kTileGreen = 0.20f;
constexpr float kTileBlue = 0.14f;
constexpr float kTileRoughness = 0.72f;

double DrapeCellM(size_t rung) {
  double cellM = kFinestCellM;
  for (size_t step = 0; step < rung; ++step) { cellM *= kCellPerRung; }
  return cellM;
}

constexpr int kClassPasses = 4;

[[nodiscard]] uint64_t EdgeKey(uint32_t a, uint32_t b) {
  return a < b ? (static_cast<uint64_t>(a) << 32U) | b : (static_cast<uint64_t>(b) << 32U) | a;
}

void Divided(std::span<const uint32_t, 3> face,
             std::span<const uint32_t, 3> cut,
             std::vector<uint32_t> &into) {
  const auto lay = [&into](uint32_t a, uint32_t b, uint32_t c) {
    into.push_back(a);
    into.push_back(b);
    into.push_back(c);
  };
  const int cuts =
      (cut[0] != kNoVertex ? 1 : 0) + (cut[1] != kNoVertex ? 1 : 0) + (cut[2] != kNoVertex ? 1 : 0);
  if (cuts == 0) {
    lay(face[0], face[1], face[2]);
    return;
  }
  if (cuts == 3) {
    lay(face[0], cut[0], cut[2]);
    lay(cut[0], face[1], cut[1]);
    lay(cut[2], cut[1], face[2]);
    lay(cut[0], cut[1], cut[2]);
    return;
  }
  if (cuts == 1) {
    for (int edge = 0; edge < 3; ++edge) {
      if (cut[edge] == kNoVertex) { continue; }
      lay(face[edge], cut[edge], face[(edge + 2) % 3]);
      lay(cut[edge], face[(edge + 1) % 3], face[(edge + 2) % 3]);
    }
    return;
  }
  for (int edge = 0; edge < 3; ++edge) {
    if (cut[edge] != kNoVertex) { continue; }
    lay(face[edge], face[(edge + 1) % 3], cut[(edge + 1) % 3]);
    lay(face[edge], cut[(edge + 1) % 3], cut[(edge + 2) % 3]);
    lay(cut[(edge + 2) % 3], cut[(edge + 1) % 3], face[(edge + 2) % 3]);
    return;
  }
}

[[nodiscard]] uint64_t WayEndKey(double latDeg, double lonDeg) {
  constexpr int64_t kBias = 0x20000000;
  const auto y = static_cast<int64_t>(std::llround(latDeg * 100000.0));
  const auto x = static_cast<int64_t>(std::llround(lonDeg * 100000.0));
  return (static_cast<uint64_t>(y + kBias) << 32U) | static_cast<uint64_t>(x + kBias);
}

class Instancing final : public Generators::DrawSink {
public:
  explicit Instancing(std::vector<Surrounds::Standing> &into) : Into_(&into) {}

  [[nodiscard]] bool Add(Generators::BodyId body,
                         Generators::ClusterId cluster,
                         const Generators::Instance &instance) noexcept override {
    if (Full()) { return false; }
    Into_->push_back(
        {.Body = body.Index(), .Cluster = static_cast<uint32_t>(cluster), .Where = instance});
    return true;
  }

  [[nodiscard]] bool Full() const noexcept override { return Into_->size() >= kMostInstances; }

private:
  static constexpr size_t kMostInstances = 1u << 20u;
  std::vector<Surrounds::Standing> *Into_;
};

} // namespace

void Engine::State::WhereTheEyeStands(double &atLat, double &atLon) const {
  const Sim::Corridor &way = Ticking.Drive.Way;
  const bool overADrive = Ticking.Drove && !way.Fine.empty();
  const double anchorLat = overADrive ? way.FrameLat : Session.Declared.Ground.Origin.LatitudeDeg;
  const double anchorLon = overADrive ? way.FrameLon : Session.Declared.Ground.Origin.LongitudeDeg;
  atLat = anchorLat;
  atLon = anchorLon;
  if (Picture.Standing == nullptr || !Picture.Standing->Watched()) { return; }
  const TangentFrame anchored = TangentFrame::At(anchorLat, anchorLon);
  const Vec3 &eye = Picture.Standing->Watching().EyeM;
  Vec3 held;
  for (int axis = 0; axis < 3; ++axis) {
    held[axis] = anchored.OriginEcef()[axis] + eye[0] * anchored.EastEcef()[axis] +
                 eye[1] * anchored.UpEcef()[axis] - eye[2] * anchored.NorthEcef()[axis];
  }
  const Ground::Geo above =
      Ground::EcefToGeoWgs84(Ground::Ecef{.X = held[0], .Y = held[1], .Z = held[2]});
  atLat = above.LatitudeDeg;
  atLon = above.LongitudeDeg;
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
  const Generators::Tile region = Generators::Tile::Of(World.Stack.Vectors()->Zoom(), atLat, atLon);
  Generators::Fields stands;
  stands.Vectors = World.Stack.Vectors();
  stands.Footprints = &World.Stack.Footprints();
  stands.WaterBodies = &World.Stack.WaterBodies();
  stands.Ways = &World.Stack.Ways();
  Generators::Ground::Snapshot snapshot;
  const Generators::Snapped how = Generators::SnapshotOver(
      region, World.Stack.Ground(), World.Stack.Classes(), stands, World.Table, &snapshot);
  World.Reached = kBaseSnapshotRows + (snapshot.Patch ? 1 : 0) + (snapshot.Classes ? 2 : 0) +
                  (snapshot.Features ? 4 : 0);
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
  const std::optional<Generators::Ground> over = Generators::Ground::Of(region, snapshot);
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
    notes[at].assign(stood.NoteNames().Size(), Generators::Yield::Note{});
    yields.emplace_back(lease->Sink(),
                        stood.NoteNames(),
                        Span<Generators::Yield::Note>(notes[at].data(), notes[at].size()));
  }
  placing.Occupy(*over, Span<Generators::Yield>(yields.data(), yields.size()));
  for (const Generators::Yield &one : yields) { World.Placed += one.Placed().Count; }
  Published.Places("generators: bodies they placed", static_cast<double>(World.Placed), "bodies");
  Published.Places(
      "generators: makers that were asked", static_cast<double>(placing.Count()), "makers");
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

bool Engine::State::Composes() {
  const Heap::Tagged composing("world-compose");
  World.GroundTiles = 0;
  if (!Picture.Standing) {
    Error = "nothing stands to compose a world around";
    return false;
  }
  const Scenario::Document &declared = Session.Declared;
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
    const double hereM = World.Stack.Ground().At(atLat, atLon).AslM().value_or(0.0);
    const double eastM = World.Stack.Ground().At(atLat, atLon + kEastStepDeg).AslM().value_or(0.0);
    Published.Places("ground: the relief says this at the origin", hereM, "m");
    Published.Places("ground: and this a kilometre east", eastM, "m");
  }

  if (!Session.Declared.Ground.Osm.empty()) {
    std::vector<Ground::OsmField::Declared> told;
    told.reserve(Session.Declared.Ground.Osm.size());
    for (const Scenario::Structure &one : Session.Declared.Ground.Osm) {
      Ground::OsmField::Declared made;
      const bool wet = one.Kind == "water";
      const Ground::OsmLayer holds =
          one.Area ? (wet ? Ground::OsmLayer::WaterPolygons : Ground::OsmLayer::Buildings)
                   : (wet ? Ground::OsmLayer::WaterLines : Ground::OsmLayer::Streets);
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

  World.Stack.ShapesFootprintsWith(&World.Shaper);
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
  const Sim::Corridor &way = Ticking.Drive.Way;
  const bool overADrive = Ticking.Drove && !way.Fine.empty();
  if (!declared.Ground.Declared && !overADrive) { return true; }
  if (!Picture.Standing || !World.Stack.Opened()) { return true; }
  Around over;
  over.LatitudeDeg = overADrive ? way.FrameLat : declared.Ground.Origin.LatitudeDeg;
  over.LongitudeDeg = overADrive ? way.FrameLon : declared.Ground.Origin.LongitudeDeg;
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
  World.Stack.Pool().Focus(over.LatitudeDeg, over.LongitudeDeg);
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

namespace {

size_t CarryIntoTheFrame(const std::vector<float> &corners,
                         const Vec3 &anchor,
                         const TangentFrame &standing,
                         std::vector<float> &places,
                         std::vector<float> &turned,
                         size_t already) {
  const size_t count = corners.size() / kTileVertexFloats;
  if (already > count || places.size() != already * 3 || turned.size() != already * 3) {
    already = 0;
  }
  places.resize(count * 3);
  turned.resize(count * 3);
  for (size_t at = already; at < count; ++at) {
    const float *const one = corners.data() + at * kTileVertexFloats;
    const Vec3 held = {{anchor[0] + static_cast<double>(one[0]),
                        anchor[1] + static_cast<double>(one[1]),
                        anchor[2] + static_cast<double>(one[2])}};
    double eastM = 0.0;
    double upM = 0.0;
    double northM = 0.0;
    standing.Place(held, &eastM, &upM, &northM);
    places[at * 3] = static_cast<float>(eastM);
    places[at * 3 + 1] = static_cast<float>(upM);
    places[at * 3 + 2] = static_cast<float>(-northM);
    const Vec3 aim = {
        {static_cast<double>(one[5]), static_cast<double>(one[6]), static_cast<double>(one[7])}};
    double alongEast = 0.0;
    double alongUp = 0.0;
    double alongNorth = 0.0;
    standing.Turn(aim, &alongEast, &alongUp, &alongNorth);
    turned[at * 3] = static_cast<float>(alongEast);
    turned[at * 3 + 1] = static_cast<float>(alongUp);
    turned[at * 3 + 2] = static_cast<float>(-alongNorth);
  }
  return count;
}
} // namespace

namespace {

void CensusOverEveryTriangle(Core::Ledger &Published,
                             std::chrono::steady_clock::time_point &censusAt,
                             std::span<const float> wallPlaces,
                             std::span<const float> wallFacing,
                             std::span<const uint32_t> wallRun,
                             std::span<const float> roofPlaces,
                             std::span<const float> roofFacing,
                             std::span<const uint32_t> roofRun) {
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

  struct AtCm {
    int64_t X = 0, Y = 0, Z = 0;

    bool operator==(const AtCm &other) const noexcept {
      return X == other.X && Y == other.Y && Z == other.Z;
    }
  };

  struct AtCmHash {
    size_t operator()(const AtCm &of) const noexcept {
      uint64_t mixed = kDigestBasis;
      mixed = (mixed ^ static_cast<uint64_t>(of.X)) * kDigestPrime;
      mixed = (mixed ^ static_cast<uint64_t>(of.Y)) * kDigestPrime;
      mixed = (mixed ^ static_cast<uint64_t>(of.Z)) * kDigestPrime;
      return static_cast<size_t>(mixed);
    }
  };

  std::unordered_map<AtCm, uint32_t, AtCmHash> seenAt;
  std::vector<uint32_t> welded;
  welded.reserve(vertices);
  size_t coincident = 0;
  for (size_t one = 0; one < vertices; ++one) {
    const float *const held = placeAt(one);
    const auto cx = static_cast<int64_t>(std::llround(static_cast<double>(held[0]) * 100.0));
    const auto cy = static_cast<int64_t>(std::llround(static_cast<double>(held[1]) * 100.0));
    const auto cz = static_cast<int64_t>(std::llround(static_cast<double>(held[2]) * 100.0));
    const AtCm key{.X = cx, .Y = cy, .Z = cz};
    const auto found = seenAt.find(key);
    if (found == seenAt.end()) {
      const auto made = static_cast<uint32_t>(seenAt.size());
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
    const std::array<uint32_t, 3> corner = {
        {welded[cornerOf(tri, 0)], welded[cornerOf(tri, 1)], welded[cornerOf(tri, 2)]}};
    if (corner[0] == corner[1] || corner[1] == corner[2] || corner[2] == corner[0]) {
      ++degenerate;
      continue;
    }
    for (int side = 0; side < 3; ++side) {
      const uint32_t from = corner[side];
      const uint32_t to = corner[(side + 1) % 3];
      const uint64_t low = from < to ? from : to;
      const uint64_t high = from < to ? to : from;
      edges[(low << 32U) | high] += 1;
    }
  }
  size_t open = 0;
  size_t overused = 0;
  for (const auto &one : edges) {
    if (one.second == 1) {
      ++open;
    } else if (one.second > 2) {
      ++overused;
    }
  }
  {
    struct Corner {
      std::array<uint32_t, 6> Bits = {{0, 0, 0, 0, 0, 0}};

      bool operator==(const Corner &other) const noexcept {
        for (size_t part = 0; part < 6; ++part) {
          if (Bits[part] != other.Bits[part]) { return false; }
        }
        return true;
      }
    };

    struct CornerHash {
      size_t operator()(const Corner &of) const noexcept {
        size_t mixed = kDigestBasis;
        for (const uint32_t one : of.Bits) { mixed = (mixed ^ one) * kDigestPrime; }
        return mixed;
      }
    };

    std::unordered_map<Corner, uint32_t, CornerHash> whole;
    size_t exact = 0;
    for (size_t one = 0; one < vertices; ++one) {
      const float *const held = placeAt(one);
      const float *const aim = turnAt(one);
      Corner key;
      for (size_t part = 0; part < 3; ++part) {
        key.Bits[part] = std::bit_cast<uint32_t>(held[part]);
        key.Bits[part + 3] = std::bit_cast<uint32_t>(aim[part]);
      }
      if (whole.emplace(key, static_cast<uint32_t>(whole.size())).second) { continue; }
      ++exact;
    }
    Published.Places("solid: building corners identical in POSITION AND NORMAL",
                     static_cast<double>(exact),
                     "corners");
    Published.Places(
        "solid: and how many distinct ones remain", static_cast<double>(whole.size()), "corners");
  }
  Published.Places("solid: building vertices welded away as coincident",
                   static_cast<double>(coincident),
                   "vertices");
  Published.Places(
      "solid: building vertices standing apart", static_cast<double>(seenAt.size()), "vertices");
  Published.Places(
      "rebuild: of that, welding and counting edges",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - censusAt)
          .count(),
      "ms");
  censusAt = std::chrono::steady_clock::now();
  Published.Places("solid: building triangles with two corners in one place",
                   static_cast<double>(degenerate),
                   "triangles");
  Published.Places(
      "solid: building edges on ONE triangle, so a HOLE", static_cast<double>(open), "edges");
  Published.Places("solid: building edges on MORE than two, so not a surface",
                   static_cast<double>(overused),
                   "edges");
  Published.Places("solid: building edges in all", static_cast<double>(edges.size()), "edges");
}
} // namespace

bool Engine::State::Grounds(bool alsoWhenTilesLanded) {
  const Heap::Tagged laying("world-ground");
  auto phaseAt = std::chrono::steady_clock::now();
  auto censusAt = phaseAt;
  auto wiresAt = phaseAt;
  const Scenario::Document &declared = Session.Declared;
  const Sim::Corridor &way = Ticking.Drive.Way;
  const bool overADrive = Ticking.Drove && !way.Fine.empty();
  if (!declared.Ground.Declared && !overADrive) { return true; }
  if (!Picture.Standing || !World.Stack.Opened()) { return true; }
  const double anchorLat = overADrive ? way.FrameLat : declared.Ground.Origin.LatitudeDeg;
  const double anchorLon = overADrive ? way.FrameLon : declared.Ground.Origin.LongitudeDeg;

  double atLat = anchorLat;
  double atLon = anchorLon;
  WhereTheEyeStands(atLat, atLon);
  Published.Places("the ring centres this far from the world's anchor",
                   std::hypot((atLat - anchorLat) * kMPerDegLat,
                              (atLon - anchorLon) * kMPerDegLon * std::cos(anchorLat * kDeg2Rad)),
                   "m");

  Around over;
  over.LatitudeDeg = atLat;
  over.LongitudeDeg = atLon;
  over.Zoom = World.Stack.FinestZoomOf(Data::DataKind::Elevation);
  {
    const double tileSpanM = 40075017.0 * std::cos(atLat * kDeg2Rad) / std::ldexp(1.0, over.Zoom);
    const double nearest = 4.0 * tileSpanM;
    const double wanted = declared.Ground.SightM > 0.0 ? declared.Ground.SightM : 240000.0;
    const double doublings = wanted > nearest ? std::log2(wanted / nearest) : 0.0;
    over.Levels = 1 + static_cast<int>(std::ceil(doublings));
    Published.Places("the sight a scenario declares", wanted, "m");
    Published.Places("and what one tile spans at the finest zoom", tileSpanM, "m");
    Published.Places("the elevation's own posting",
                     World.Stack.Ground().PostM(declared.Ground.Origin.LatitudeDeg),
                     "m");
    Published.Places("and the drawn mesh's vertex spacing",
                     over.Grid > 1 ? tileSpanM / static_cast<double>(over.Grid - 1) : 0.0,
                     "m");
  }
  if (!Watches()) { return false; }
  if (Picture.Standing->Watched()) {
    const Vec3 &at = Picture.Standing->Watching().EyeM;
    const TangentFrame eyed = TangentFrame::At(atLat, atLon);
    for (int axis = 0; axis < 3; ++axis) {
      over.EyeM[axis] = eyed.OriginEcef()[axis] + at[0] * eyed.EastEcef()[axis] +
                        at[1] * eyed.UpEcef()[axis] - at[2] * eyed.NorthEcef()[axis];
      over.Up[axis] = static_cast<float>(eyed.UpEcef()[axis]);
    }
  }
  if (Picture.Frame.HeightPx > 0) {
    const double halfFov = 0.5 * 55.0 * kDeg2Rad;
    over.FocalPx =
        static_cast<float>(0.5 * static_cast<double>(Picture.Frame.HeightPx) / std::tan(halfFov));
  }
  {
    const Ground::TileFrac here = Ground::ToTileFracClamped(
        Ground::Geo{.LongitudeDeg = atLon, .LatitudeDeg = atLat}, over.Zoom);
    const uint64_t from = (static_cast<uint64_t>(static_cast<int64_t>(std::floor(here.X))) << 32U) ^
                          static_cast<uint64_t>(static_cast<int64_t>(std::floor(here.Y))) ^
                          (static_cast<uint64_t>(over.Levels) << 56u);
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
    const std::shared_ptr<const ClassStructure> naming = World.Stack.Classes().Read();
    const uint64_t classes = naming ? naming->Version() : 0;
    const bool elsewhere = from != World.LaidFrom;
    const bool grew = alsoWhenTilesLanded && resident != World.LaidResident;
    const bool renamed = classes != World.LaidClasses;
    {
      const Raised &standing = World.Stack.Footprints().Built();
      const size_t triangles = (standing.WallRun.size() + standing.RoofRun.size()) / 3u;
      Published.Places(
          "building triangles the world meshed", static_cast<double>(triangles), "triangles");
    }
    Published.Places(
        "world: the bytes its fields hold", static_cast<double>(World.Stack.HeapBytes()), "bytes");
    Published.Places("world: of that, the land classes",
                     static_cast<double>(World.Stack.Classes().HeapBytes()),
                     "bytes");
    Published.Places(
        "world: the buildings", static_cast<double>(World.Stack.Footprints().HeapBytes()), "bytes");
    Published.Places(
        "world: the water", static_cast<double>(World.Stack.WaterBodies().HeapBytes()), "bytes");
    Published.Places(
        "world: the streets", static_cast<double>(World.Stack.Ways().HeapBytes()), "bytes");
    Published.Places("world: the ceiling its fields stand under",
                     static_cast<double>(Ground::GroundStack::kHoldsBytes),
                     "bytes");
    Published.Places("world: times a round stopped at that ceiling",
                     static_cast<double>(World.Stack.OverCeiling()),
                     "rounds");
    Published.Places("world: and the OSM features",
                     World.Stack.Vectors() != nullptr
                         ? static_cast<double>(World.Stack.Vectors()->HeapBytes())
                         : 0.0,
                     "bytes");
    Published.Places("tiles laid bare on the ellipsoid",
                     static_cast<double>(sees->Pending + sees->Absent + sees->Refused),
                     "tiles");
    if (World.EverLaid && !elsewhere && !grew && !renamed) { return true; }
    Published.Places(
        "rebuilds since the world stood", static_cast<double>(World.Relaid + 1u), "rebuilds");
    Published.Places("rebuild: the eye walked into another tile", elsewhere ? 1.0 : 0.0, "yes/no");
    Published.Places("rebuild: tiles resident when it did", static_cast<double>(resident), "tiles");
    Published.Places(
        "rebuild: and resident the time before", static_cast<double>(World.LaidResident), "tiles");
    Published.Places("rebuild: the land classes were named anew", renamed ? 1.0 : 0.0, "yes/no");
    World.LaidFrom = from;
    World.LaidResident = resident;
    World.LaidClasses = classes;
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
  double sank = 0.0;
  double sankAt = 0.0;
  double tallest = -kNoLeastYet;
  double lowest = kNoLeastYet;
  double tallestOut = 0.0;
  for (size_t at = 0; at + 2 < laid->PositionM.size(); at += 3) {
    const Vec3 held = {{laid->OriginEcef[0] + static_cast<double>(laid->PositionM[at]),
                        laid->OriginEcef[1] + static_cast<double>(laid->PositionM[at + 1]),
                        laid->OriginEcef[2] + static_cast<double>(laid->PositionM[at + 2])}};
    double eastM = 0.0;
    double upM = 0.0;
    double northM = 0.0;
    standing.Place(held, &eastM, &upM, &northM);
    inFrame[at] = static_cast<float>(eastM);
    inFrame[at + 1] = static_cast<float>(upM);
    inFrame[at + 2] = static_cast<float>(-northM);
    const Ground::Geo where =
        Ground::EcefToGeoWgs84(Ground::Ecef{.X = held[0], .Y = held[1], .Z = held[2]});
    const double below = where.HeightM - upM;
    if (below > sank) {
      sank = below;
      sankAt = std::sqrt(eastM * eastM + northM * northM);
    }
    if (where.HeightM > tallest) {
      tallest = where.HeightM;
      tallestOut = std::sqrt(eastM * eastM + northM * northM);
    }
    lowest = std::min(where.HeightM, lowest);
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
    double widest = 0.0;
    double leaning = 0.0;
    double leanSum = 0.0;
    size_t shared = 0;
    size_t leanCount = 0;
    for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
      const auto east = static_cast<int64_t>(std::llround(static_cast<double>(inFrame[at]) * 4.0));
      const auto south =
          static_cast<int64_t>(std::llround(static_cast<double>(inFrame[at + 2]) * 4.0));
      const auto atE = static_cast<uint64_t>(east + 0x20000000LL);
      const auto atS = static_cast<uint64_t>(south + 0x20000000LL);
      const uint64_t key = (atE << 32U) | atS;
      const auto stood = met.find(key);
      if (stood == met.end()) {
        met.emplace(key, inFrame[at + 1]);
        met2.emplace(key, at);
        continue;
      }
      ++shared;
      const double apart =
          std::fabs(static_cast<double>(inFrame[at + 1]) - static_cast<double>(stood->second));
      widest = std::max(apart, widest);
      if (at + 2 < laid->NormalM.size() && stood->second == inFrame[at + 1]) {
        const size_t twin = met2[key];
        double dot = 0.0;
        double one = 0.0;
        double two = 0.0;
        for (size_t axis = 0; axis < 3; ++axis) {
          const auto a = static_cast<double>(laid->NormalM[at + axis]);
          const auto b = static_cast<double>(laid->NormalM[twin + axis]);
          dot += a * b;
          one += a * a;
          two += b * b;
        }
        if (one > 0.0 && two > 0.0) {
          const double leanDeg =
              std::acos(std::fmin(1.0, std::fmax(-1.0, dot / std::sqrt(one * two)))) *
              kDegPerHalfTurn / std::numbers::pi;
          leaning = std::max(leaning, leanDeg);
          leanSum += leanDeg;
          ++leanCount;
        }
      }
    }
    Published.Places(
        "vertices two tiles put in the same place", static_cast<double>(shared), "vertices");
    Published.Places("and the widest they disagree on height", widest, "m");
    Published.Places("the widest their NORMALS disagree", leaning, "deg");
    Published.Places("and how far those disagree on average",
                     leanCount > 0 ? leanSum / static_cast<double>(leanCount) : 0.0,
                     "deg");
  }
  std::vector<float> tinted;
  std::vector<float> classUv;
  std::vector<float> classPalette;
  std::shared_ptr<const ClassStructure> classStructure;
  {
    const std::shared_ptr<const ClassStructure> classes = World.Stack.Classes().Read();
    const Ground::VegetationTemplates &wearing = World.Stack.Vegetation();
    std::vector<int> classOf;
    std::vector<double> atGeo;
    size_t classDivided = 0;
    const Render::Medium fallback = Render::kEarthAir;
    size_t named = 0;
    if (classes && wearing.Ready()) {
      classStructure = classes;
      classPalette.assign(4u + (wearing.TemplateCount() + 1u) * 4u, 0.0f);
      const auto rows = static_cast<uint32_t>(wearing.TemplateCount());
      classPalette[0] = std::bit_cast<float>(rows);
      for (size_t row = 0; row < wearing.TemplateCount(); ++row) {
        for (int channel = 0; channel < 3; ++channel) {
          classPalette[4u + row * 4u + static_cast<size_t>(channel)] =
              wearing.Rows()[row].Ground[channel];
        }
        classPalette[4u + row * 4u + 3u] = wearing.Rows()[row].Mix[2];
      }
      for (int channel = 0; channel < 3; ++channel) {
        classPalette[4u + wearing.TemplateCount() * 4u + static_cast<size_t>(channel)] =
            fallback.GroundAlbedo[channel];
      }
      classPalette[4u + wearing.TemplateCount() * 4u + 3u] = 0.0f;
      tinted.resize((inFrame.size() / 3) * 4);
      classUv.resize((inFrame.size() / 3) * 2);
      for (size_t at = 0, one = 0; at + 2 < laid->PositionM.size(); at += 3, ++one) {
        const Vec3 held = {{laid->OriginEcef[0] + static_cast<double>(laid->PositionM[at]),
                            laid->OriginEcef[1] + static_cast<double>(laid->PositionM[at + 1]),
                            laid->OriginEcef[2] + static_cast<double>(laid->PositionM[at + 2])}};
        const Ground::Geo where =
            Ground::EcefToGeoWgs84(Ground::Ecef{.X = held[0], .Y = held[1], .Z = held[2]});
        double edgeM = 0.0;
        int second = -1;
        const int which = World.Stack.Classes().ClassAt(
            *classes, where.LatitudeDeg, where.LongitudeDeg, &edgeM, &second);
        {
          double eastM = 0.0;
          double northM = 0.0;
          World.Stack.Classes().ToEnu(where.LatitudeDeg, where.LongitudeDeg, &eastM, &northM);
          classUv[one * 2] = static_cast<float>(eastM);
          classUv[one * 2 + 1] = static_cast<float>(northM);
        }
        const bool stands = which >= 0 && static_cast<size_t>(which) < wearing.TemplateCount();
        if (stands) { ++named; }
        const Ground::VegetationTemplates::Row &wore =
            wearing.Rows()[stands ? static_cast<size_t>(which) : 0];
        tinted[one * 4] = stands ? wore.Ground[0] : fallback.GroundAlbedo[0];
        tinted[one * 4 + 1] = stands ? wore.Ground[1] : fallback.GroundAlbedo[1];
        tinted[one * 4 + 2] = stands ? wore.Ground[2] : fallback.GroundAlbedo[2];
        tinted[one * 4 + 3] = 1.0f;
        classOf.push_back(which);
        atGeo.push_back(where.LatitudeDeg);
        atGeo.push_back(where.LongitudeDeg);
      }
      for (int pass = 0; pass < kClassPasses; ++pass) {
        std::unordered_map<uint64_t, uint32_t> split;
        for (size_t at = 0; at + 2 < laid->Index.size(); at += 3) {
          for (int edge = 0; edge < 3; ++edge) {
            const uint32_t a = laid->Index[at + static_cast<size_t>(edge)];
            const uint32_t b = laid->Index[at + static_cast<size_t>((edge + 1) % 3)];
            if (classOf[a] == classOf[b]) { continue; }
            split.emplace(EdgeKey(a, b), kNoVertex);
          }
        }
        if (split.empty()) { break; }
        const auto halve = [&](uint32_t a, uint32_t b) {
          const auto found = split.find(EdgeKey(a, b));
          if (found == split.end()) { return kNoVertex; }
          if (found->second != kNoVertex) { return found->second; }
          const auto made = static_cast<uint32_t>(inFrame.size() / 3u);
          for (int axis = 0; axis < 3; ++axis) {
            inFrame.push_back(0.5f *
                              (inFrame[static_cast<size_t>(a) * 3u + static_cast<size_t>(axis)] +
                               inFrame[static_cast<size_t>(b) * 3u + static_cast<size_t>(axis)]));
            laid->NormalM.push_back(
                0.5f * (laid->NormalM[static_cast<size_t>(a) * 3u + static_cast<size_t>(axis)] +
                        laid->NormalM[static_cast<size_t>(b) * 3u + static_cast<size_t>(axis)]));
          }
          const double lat =
              0.5 * (atGeo[static_cast<size_t>(a) * 2u] + atGeo[static_cast<size_t>(b) * 2u]);
          const double lon = 0.5 * (atGeo[static_cast<size_t>(a) * 2u + 1u] +
                                    atGeo[static_cast<size_t>(b) * 2u + 1u]);
          atGeo.push_back(lat);
          atGeo.push_back(lon);
          double edgeM = 0.0;
          int second = -1;
          const int names = World.Stack.Classes().ClassAt(*classes, lat, lon, &edgeM, &second);
          classOf.push_back(names);
          double eastM = 0.0;
          double northM = 0.0;
          World.Stack.Classes().ToEnu(lat, lon, &eastM, &northM);
          classUv.push_back(static_cast<float>(eastM));
          classUv.push_back(static_cast<float>(northM));
          const bool named = names >= 0 && static_cast<size_t>(names) < wearing.TemplateCount();
          const Ground::VegetationTemplates::Row &wore =
              wearing.Rows()[named ? static_cast<size_t>(names) : 0];
          for (int channel = 0; channel < 3; ++channel) {
            tinted.push_back(named ? wore.Ground[channel]
                                   : fallback.GroundAlbedo[static_cast<size_t>(channel)]);
          }
          tinted.push_back(1.0f);
          found->second = made;
          return made;
        };
        std::vector<uint32_t> finer;
        finer.reserve(laid->Index.size() * 2u);
        for (size_t at = 0; at + 2 < laid->Index.size(); at += 3) {
          const std::array<uint32_t, 3> face = {
              {laid->Index[at], laid->Index[at + 1u], laid->Index[at + 2u]}};
          const std::array<uint32_t, 3> cut = {
              {halve(face[0], face[1]), halve(face[1], face[2]), halve(face[2], face[0])}};
          Divided(face, cut, finer);
        }
        classDivided += (finer.size() - laid->Index.size()) / 3u;
        laid->Index.swap(finer);
      }
      Published.Places("class field: triangles the boundary divided",
                       static_cast<double>(classDivided),
                       "triangles");
    }
    if (!tinted.empty()) {
      Vec3 wornSum;
      const size_t worn = tinted.size() / 4;
      for (size_t one = 0; one < worn; ++one) {
        for (int channel = 0; channel < 3; ++channel) {
          wornSum[channel] += static_cast<double>(tinted[one * 4 + static_cast<size_t>(channel)]);
        }
      }
      const Vec3 wornMean = {{wornSum[0] / static_cast<double>(worn),
                              wornSum[1] / static_cast<double>(worn),
                              wornSum[2] / static_cast<double>(worn)}};
      Picture.Standing->Grounding(wornMean);
      Published.Places(
          "lighting: the ground it bounces off, red", kPerMille * wornMean[0], "albedo/1000");
      Published.Places("lighting: green", kPerMille * wornMean[1], "albedo/1000");
      Published.Places("lighting: blue", kPerMille * wornMean[2], "albedo/1000");
    }
    const Render::SubjectEnvironment &lighting = Picture.Standing->AmbientStanding();
    Published.Places("lighting: the sky's own radiance, red", lighting.RadianceLinear[0], "cd/m2");
    Published.Places("lighting: sky green", lighting.RadianceLinear[1], "cd/m2");
    Published.Places("lighting: sky blue", lighting.RadianceLinear[2], "cd/m2");
    Published.Places(
        "lighting: the ground's bounced radiance, red", lighting.GroundLinear[0], "cd/m2");
    Published.Places("lighting: bounce green", lighting.GroundLinear[1], "cd/m2");
    Published.Places("lighting: bounce blue", lighting.GroundLinear[2], "cd/m2");
    Published.Places(
        "class field: the vegetation table is ready", wearing.Ready() ? 1.0 : 0.0, "yes/no");
    Published.Places("class field: rows the table carries",
                     static_cast<double>(wearing.TemplateCount()),
                     "rows");
    Published.Places("class field: features the fine tier holds",
                     static_cast<double>(World.Stack.Classes().FeaturesHeld()),
                     "features");
    Published.Places("class field: of those it has taken",
                     static_cast<double>(World.Stack.Classes().FeaturesTaken()),
                     "features");
    Published.Places(
        "the ring's vertices a land class names", static_cast<double>(named), "vertices");
    Published.Places("class field: it published a structure", classes ? 1.0 : 0.0, "yes/no");
    Published.Places("class field: the version the colours used",
                     classes ? static_cast<double>(classes->Version()) : -1.0,
                     "version");
    Published.Places("class field: it calls itself complete",
                     World.Stack.Classes().Complete() ? 1.0 : 0.0,
                     "yes/no");
    Published.Places("class field: tiles it waits for",
                     static_cast<double>(World.Stack.Classes().PendingTiles()),
                     "tiles");
    Published.Places("class field: the fraction it has no data for",
                     classes ? classes->NoDataFraction() : -1.0,
                     "fraction");
    Published.Places(
        "class field: the materials are loaded", wearing.Ready() ? 1.0 : 0.0, "yes/no");
    Published.Places("out of, for a class", static_cast<double>(inFrame.size()) / 3.0, "vertices");
  }
  Published.Places("the ring's vertex that sinks furthest below its own altitude", sank, "m");
  Published.Places("and how far out it lies", sankAt, "m");
  Published.Places("a sphere would sink it by", sankAt * sankAt / (2.0 * Data::kWgs84A), "m");

  {
    double nearest = kBeyondAnyCoordinate;
    double atUp = 0.0;
    double farthest = 0.0;
    double farUp = 0.0;
    for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
      const auto east = static_cast<double>(inFrame[at]);
      const auto south = static_cast<double>(inFrame[at + 2]);
      const double away = east * east + south * south;
      if (away < nearest) {
        nearest = away;
        atUp = static_cast<double>(inFrame[at + 1]);
      }
      if (away > farthest) {
        farthest = away;
        farUp = static_cast<double>(inFrame[at + 1]);
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
      const Vec3 held = {{static_cast<double>(laid->NormalM[at]),
                          static_cast<double>(laid->NormalM[at + 1]),
                          static_cast<double>(laid->NormalM[at + 2])}};
      double alongEast = 0.0;
      double alongUp = 0.0;
      double alongNorth = 0.0;
      standing.Turn(held, &alongEast, &alongUp, &alongNorth);
      laid->NormalM[at] = static_cast<float>(alongEast);
      laid->NormalM[at + 1] = static_cast<float>(alongUp);
      laid->NormalM[at + 2] = static_cast<float>(-alongNorth);
    }
  }
  {
    double up = 0.0;
    double down = 0.0;
    double sideways = 0.0;
    double unlengthed = 0.0;
    for (size_t at = 0; at + 2 < laid->NormalM.size(); at += 3) {
      const double x = laid->NormalM[at];
      const double y = laid->NormalM[at + 1];
      const double z = laid->NormalM[at + 2];
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
      double steepest = 0.0;
      double mean = 0.0;
      double counted = 0.0;
      for (size_t at = 0; at + 2 < laid->NormalM.size(); at += 3) {
        const double x = laid->NormalM[at];
        const double y = laid->NormalM[at + 1];
        const double z = laid->NormalM[at + 2];
        const double length = std::sqrt(x * x + y * y + z * z);
        if (!(length > kLeastRunM)) { continue; }
        const double leanDeg = std::acos(std::fmin(1.0, y / length)) * kRad2Deg;
        steepest = leanDeg > steepest ? leanDeg : steepest;
        mean += leanDeg;
        counted += 1.0;
      }
      Published.Places("the steepest the ring's surface leans", steepest, "deg");
      Published.Places("how far it leans on average", counted > 0.0 ? mean / counted : 0.0, "deg");
    }
    Published.Places("its normals with no length at all", unlengthed, "normals");
    Published.Places(
        "its normals in all", static_cast<double>(laid->NormalM.size() / 3), "normals");
    {
      double least = kBeyondAnyCoordinate;
      double most = -kBeyondAnyCoordinate;
      const std::vector<float> &held = overADrive ? inFrame : laid->PositionM;
      for (size_t at = 1; at < held.size(); at += 3) {
        const auto y = static_cast<double>(held[at]);
        least = std::min(least, y);
        most = std::max(most, y);
      }
      Published.Places("the ground ring's lowest vertex", least, "m");
      Published.Places("the ground ring's highest", most, "m");
    }
  }
  Geometry ground;
  Material bare;
  {
    const Render::Medium held = Render::kEarthAir;
    for (int channel = 0; channel < 3; ++channel) {
      bare.BaseColour[channel] = held.GroundAlbedo[channel];
    }
  }
  constexpr double kRoadStepM = 16.0;
  constexpr double kNodeSnapM = 2.0;
  constexpr double kCrossCellM = 32.0;
  constexpr double kMeetsWithinM = 10.0;
  constexpr int kLevelPasses = 24;
  constexpr double kLevelledM = 0.01;
  constexpr int kRampPasses = 12;
  constexpr int kChordPasses = 4;
  constexpr double kChordWithinM = 0.20;
  constexpr double kLeastCrestK = 10.0;
  constexpr double kPadApronM = 6.0;
  constexpr double kVergeM = 1.5;
  constexpr double kBatterRun = 1.5;
  constexpr double kLeastApronM = 3.0;
  constexpr double kMostApronM = 240.0;
  constexpr double kFinestGroundM = 3.0;
  constexpr size_t kMostYieldTriangles = 24000;
  constexpr double kFlyingM = 1.0;
  constexpr size_t kDrapeRungs = 6;
  constexpr double kTrimMostWidths = 4.0;
  constexpr double kFitWithinM = 0.5;
  constexpr double kStampWorthM = 0.25;
  constexpr double kBrokenGroundM = 1.0;
  constexpr double kGroundCellM = 25.0;
  constexpr double kFitTightestM = 5.5;
  constexpr double kLeastRoadM = 2.0;
  constexpr double kDrapeGridM = 32.0;
  if (!tinted.empty()) {
    for (int channel = 0; channel < 3; ++channel) { bare.BaseColour[channel] = 1.0f; }
  }
  const MaterialInstance ringSurface = ground.addSurface("ground", bare);
  const int ringPart = ground.addPart("ground", ringSurface);

  {
    const Ground::BuildingField &prints = World.Stack.Footprints();
    const Raised &built = prints.Built();
    const Vec3 &anchor = prints.Anchor();
    {
      double away = 0.0;
      for (int axis = 0; axis < 3; ++axis) {
        const double step = anchor[axis] - standing.OriginEcef()[axis];
        away += step * step;
      }
      Published.Places(
          "buildings: their anchor lies from the frame's origin", std::sqrt(away), "m");
      Published.Places("buildings: floats in the soup",
                       static_cast<double>(built.WallCorners.size() + built.RoofCorners.size()),
                       "floats");
      Published.Places("buildings: the field's last delta began at",
                       static_cast<double>(prints.AddedFirst()),
                       "floats");
      Published.Places(
          "buildings: and ran for", static_cast<double>(prints.AddedCount()), "floats");
      {
        std::vector<double> fill = prints.SeatSpreadM();
        std::vector<double> across = prints.FootprintAcrossM();
        if (!fill.empty()) {
          std::ranges::sort(fill);
          std::ranges::sort(across);
          const auto pick = [](const std::vector<double> &of, double part) {
            return of[static_cast<size_t>(static_cast<double>(of.size() - 1u) * part)];
          };
          size_t wouldStamp = 0;
          for (const double filled : fill) {
            if (filled > kStampWorthM) { ++wouldStamp; }
          }
          size_t underOneCell = 0;
          for (const double wide : across) {
            if (wide < kGroundCellM) { ++underOneCell; }
          }
          Published.Places("buildings: a stamp would fill, p50", pick(fill, 0.5), "m");
          Published.Places("buildings: a stamp would fill, p95", pick(fill, kBroadQuantile), "m");
          Published.Places("buildings: a stamp would fill, worst", fill.back(), "m");
          Published.Places(
              "buildings: footprints worth a stamp", static_cast<double>(wouldStamp), "footprints");
          Published.Places("buildings: footprint across, p50", pick(across, 0.5), "m");
          Published.Places("buildings: footprints narrower than a ground cell",
                           static_cast<double>(underOneCell),
                           "footprints");
        }
      }
      Published.Places("buildings: footprints the field holds",
                       static_cast<double>(prints.Footprints().size()),
                       "footprints");
      if (World.Stack.Vectors() != nullptr) {
        Published.Places("buildings: vector tiles the field settled",
                         static_cast<double>(World.Stack.Vectors()->Tiles().size()),
                         "tiles");
        Published.Places("buildings: OSM features it holds",
                         static_cast<double>(World.Stack.Vectors()->Features().size()),
                         "features");
      }
      {
        double least = kBeyondAnyCoordinate;
        double most = -kBeyondAnyCoordinate;
        size_t within = 0;
        for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
          const auto east = static_cast<double>(inFrame[at]);
          const auto south = static_cast<double>(inFrame[at + 2]);
          if (east * east + south * south > kFootprintReachM * kFootprintReachM) { continue; }
          const auto up = static_cast<double>(inFrame[at + 1]);
          least = up < least ? up : least;
          most = up > most ? up : most;
          ++within;
        }
        Published.Places(
            "buildings: the ring within 3.2 km runs from", within > 0 ? least : 0.0, "m up");
        Published.Places("buildings: to", within > 0 ? most : 0.0, "m up");
        Published.Places(
            "buildings: over this many ring vertices", static_cast<double>(within), "vertices");
      }
    }
    const auto builtAt = std::chrono::steady_clock::now();
    if (built.WallRun.size() + built.RoofRun.size() >= 3) {
      if (World.CarriedFrom[0] != anchorLat || World.CarriedFrom[1] != anchorLon) {
        World.WallCarried = 0;
        World.RoofCarried = 0;
        World.CarriedFrom[0] = anchorLat;
        World.CarriedFrom[1] = anchorLon;
      }
      std::vector<float> &wallPlaces = World.WallPlaces;
      std::vector<float> &wallFacing = World.WallFacing;
      std::vector<float> &roofPlaces = World.RoofPlaces;
      std::vector<float> &roofFacing = World.RoofFacing;
      World.WallCarried = CarryIntoTheFrame(
          built.WallCorners, anchor, standing, wallPlaces, wallFacing, World.WallCarried);
      World.RoofCarried = CarryIntoTheFrame(
          built.RoofCorners, anchor, standing, roofPlaces, roofFacing, World.RoofCarried);
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
      walls.BaseColour[0] = kWallRed;
      walls.BaseColour[1] = kWallGreen;
      walls.BaseColour[2] = kWallBlue;
      walls.Roughness = kWallRoughness;
      Material tiles;
      tiles.BaseColour[0] = kTileRed;
      tiles.BaseColour[1] = kTileGreen;
      tiles.BaseColour[2] = kTileBlue;
      tiles.Roughness = kTileRoughness;
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
        CensusOverEveryTriangle(
            Published, censusAt, wallPlaces, wallFacing, wallRun, roofPlaces, roofFacing, roofRun);
      }
      Published.Places(
          "buildings: roof triangles", static_cast<double>(roofRun.size() / 3), "triangles");
      Published.Places(
          "buildings: wall triangles", static_cast<double>(wallRun.size() / 3), "triangles");
      {
        size_t upright = 0;
        size_t facingDown = 0;
        for (size_t one = 0; one + 2 < wallFacing.size(); one += 3) {
          const auto aloft = static_cast<double>(wallFacing[one + 1]);
          if (aloft < -0.5) {
            ++facingDown;
          } else if (aloft > -0.5 && aloft < 0.5) {
            ++upright;
          }
        }
        Published.Places(
            "buildings: wall normals standing upright", static_cast<double>(upright), "normals");
        Published.Places(
            "buildings: wall normals facing DOWN", static_cast<double>(facingDown), "normals");
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
      Published.Places(
          "buildings: the part they were given", static_cast<double>(builtPart), "index");
      Published.Places(
          "buildings: the wall surface", static_cast<double>(wallSurface.index()), "index");
      Published.Places(
          "buildings: the roof surface", static_cast<double>(roofSurface.index()), "index");
      Published.Places("buildings: positions taken", tookPlaces ? 1.0 : 0.0, "yes/no");
      Published.Places("buildings: normals taken", tookFacing ? 1.0 : 0.0, "yes/no");
      Published.Places("buildings: triangles taken", tookRun ? 1.0 : 0.0, "yes/no");
      Published.Places(
          "buildings: parts the geometry holds", static_cast<double>(ground.parts()), "parts");
      Published.Places(
          "buildings: triangles this rebuild meshed", static_cast<double>(triangles), "triangles");
      Published.Places(
          "rebuild: of that, carrying the buildings into the frame",
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - builtAt)
              .count(),
          "ms");
      Published.Places(
          "buildings: corners the soup holds", static_cast<double>(vertices), "corners");
      if (declared.Render.Audits) {
        double up = 0.0;
        double down = 0.0;
        double sideways = 0.0;
        double unlengthed = 0.0;
        const double inward = 0.0;
        for (size_t at = 0; at < vertices; ++at) {
          const float *const aim = turnAt(at);
          const double x = aim[0];
          const double y = aim[1];
          const double z = aim[2];
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
        Published.Places("buildings: normals in all", static_cast<double>(vertices), "normals");
        (void)inward;
      }
      if (declared.Render.Audits) {
        size_t needles = 0;
        size_t reaching = 0;
        double longest = 0.0;
        double furthest = 0.0;
        for (size_t tri = 0; tri < triangles; ++tri) {
          const float *const a = placeAt(cornerOf(tri, 0));
          const float *const b = placeAt(cornerOf(tri, 1));
          const float *const c = placeAt(cornerOf(tri, 2));
          const double ux = b[0] - a[0];
          const double uy = b[1] - a[1];
          const double uz = b[2] - a[2];
          const double vx = c[0] - a[0];
          const double vy = c[1] - a[1];
          const double vz = c[2] - a[2];
          const double wx = c[0] - b[0];
          const double wy = c[1] - b[1];
          const double wz = c[2] - b[2];
          const double nx = uy * vz - uz * vy;
          const double ny = uz * vx - ux * vz;
          const double nz = ux * vy - uy * vx;
          const double area = 0.5 * std::sqrt(nx * nx + ny * ny + nz * nz);
          const double edge = std::sqrt(std::max({ux * ux + uy * uy + uz * uz,
                                                  vx * vx + vy * vy + vz * vz,
                                                  wx * wx + wy * wy + wz * wz}));
          if (area < kSliverAreaM2 && edge > kSliverEdgeM) {
            ++needles;
            longest = edge > longest ? edge : longest;
          }
          if (edge > kLongEdgeM) {
            ++reaching;
            furthest = edge > furthest ? edge : furthest;
          }
        }
        Published.Places(
            "buildings: triangles that are needles", static_cast<double>(needles), "triangles");
        Published.Places("buildings: the longest edge one carries", longest, "m");
        Published.Places(
            "buildings: triangles reaching over 20 m", static_cast<double>(reaching), "triangles");
        Published.Places("buildings: the furthest any reaches", furthest, "m");
        Published.Places("buildings: roofs the clipper could not cover",
                         static_cast<double>(Generators::RoofSurface::UnclippedTaken()),
                         "roofs");
        Published.Places("buildings: roof triangles with a vertex outside their footprint",
                         static_cast<double>(Generators::RoofSurface::OutsideTaken()),
                         "triangles");
        Published.Places("buildings: seated BELOW the ground they stand on",
                         static_cast<double>(Generators::BuildingMesh::BuriedTaken()),
                         "buildings");
        Published.Places("buildings: raised with full architecture",
                         static_cast<double>(Generators::BuildingMesh::RaisedTaken()),
                         "buildings");
        Published.Places("buildings: reduced to a hull box",
                         static_cast<double>(Generators::BuildingMesh::BoxesTaken()),
                         "buildings");
        Published.Places("buildings: past even a BOX's pixel budget",
                         static_cast<double>(Generators::BuildingMesh::OverBudgetTaken()),
                         "buildings");
        Published.Places("buildings: meshed with NO pixel scale declared",
                         static_cast<double>(Generators::BuildingMesh::UnscaledTaken()),
                         "buildings");
        Published.Places("buildings: the farthest one meshed lies",
                         static_cast<double>(Generators::BuildingMesh::FarthestMTaken()),
                         "m out");
        Published.Places("buildings: and the deepest of them is buried by",
                         static_cast<double>(Generators::BuildingMesh::DeepestBuriedMmTaken()),
                         "mm");
      }
      if (declared.Render.Audits) {
        double least = kBeyondAnyCoordinate;
        double most = -kBeyondAnyCoordinate;
        double nearest = kBeyondAnyCoordinate;
        double farthest = 0.0;
        for (size_t at = 0; at < vertices; ++at) {
          const float *const held = placeAt(at);
          const auto up = static_cast<double>(held[1]);
          const auto east = static_cast<double>(held[0]);
          const auto south = static_cast<double>(held[2]);
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
      Published.Places("buildings: triangles this rebuild meshed", 0.0, "triangles");
    }
  }

  std::unordered_map<uint64_t, float> drawnGround;
  std::array<std::unordered_map<uint64_t, std::vector<uint32_t>>, kDrapeRungs> facesAt;
  std::vector<Yields> corridor;
  Generators::RoadRaised pavement;
  {
    Published.Places(
        "rebuild: of that, the ring and the buildings into the frame",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - censusAt)
            .count(),
        "ms");
    censusAt = std::chrono::steady_clock::now();
    std::unordered_map<uint64_t, std::pair<double, uint32_t>> summed;
    summed.reserve(inFrame.size() / 3);
    for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
      const auto east =
          static_cast<int64_t>(std::llround(static_cast<double>(inFrame[at]) / kDrapeGridM));
      const auto south =
          static_cast<int64_t>(std::llround(static_cast<double>(inFrame[at + 2]) / kDrapeGridM));
      const auto atE = static_cast<uint64_t>(east + 0x20000000LL);
      const auto atS = static_cast<uint64_t>(south + 0x20000000LL);
      const uint64_t key = (atE << 32U) | atS;
      std::pair<double, uint32_t> &cell = summed[key];
      cell.first += static_cast<double>(inFrame[at + 1]);
      ++cell.second;
    }
    drawnGround.reserve(summed.size());
    size_t crowded = 0;
    for (const auto &one : summed) {
      drawnGround[one.first] =
          static_cast<float>(one.second.first / static_cast<double>(one.second.second));
      if (one.second.second > 1) { ++crowded; }
    }
    Published.Places("ring: vertices the drape grid holds",
                     static_cast<double>(inFrame.size()) / 3.0,
                     "vertices");
    Published.Places("ring: cells they fall into", static_cast<double>(summed.size()), "cells");
    Published.Places("ring: cells holding more than one", static_cast<double>(crowded), "cells");

    facesAt[0].reserve(laid->Index.size() / 3u);
    std::array<size_t, kDrapeRungs> rungTaken = {{}};
    for (size_t one = 0; one + 2 < laid->Index.size(); one += 3) {
      double lowE = kBeyondAnyCoordinate;
      double highE = -kBeyondAnyCoordinate;
      double lowS = kBeyondAnyCoordinate;
      double highS = -kBeyondAnyCoordinate;
      bool whole = true;
      for (size_t corner = 0; corner < 3; ++corner) {
        const size_t held = static_cast<size_t>(laid->Index[one + corner]) * 3u;
        if (held + 2 >= inFrame.size()) {
          whole = false;
          break;
        }
        lowE = std::min(lowE, static_cast<double>(inFrame[held]));
        highE = std::max(highE, static_cast<double>(inFrame[held]));
        lowS = std::min(lowS, static_cast<double>(inFrame[held + 2]));
        highS = std::max(highS, static_cast<double>(inFrame[held + 2]));
      }
      if (!whole) { continue; }
      const double across = std::max(highE - lowE, highS - lowS);
      size_t rung = 0;
      while (rung + 1u < kDrapeRungs && across > 2.0 * DrapeCellM(rung)) { ++rung; }
      const double cellM = DrapeCellM(rung);
      const auto fromE = static_cast<int64_t>(std::floor(lowE / cellM));
      const auto toE = static_cast<int64_t>(std::floor(highE / cellM));
      const auto fromS = static_cast<int64_t>(std::floor(lowS / cellM));
      const auto toS = static_cast<int64_t>(std::floor(highS / cellM));
      if ((toE - fromE + 1) * (toS - fromS + 1) > 64) { continue; }
      ++rungTaken[rung];
      for (int64_t cellE = fromE; cellE <= toE; ++cellE) {
        for (int64_t cellS = fromS; cellS <= toS; ++cellS) {
          const auto atE = static_cast<uint64_t>(cellE + 0x20000000LL);
          const auto atS = static_cast<uint64_t>(cellS + 0x20000000LL);
          facesAt[rung][(atE << 32U) | atS].push_back(static_cast<uint32_t>(one));
        }
      }
    }
    for (size_t rung = 0; rung < kDrapeRungs; ++rung) {
      Published.Places(std::string("ring: drape triangles on rung ") +
                           static_cast<char>('0' + rung),
                       static_cast<double>(rungTaken[rung]),
                       "triangles");
    }
    Published.Places("ring: triangles the drape can reach",
                     static_cast<double>(laid->Index.size()) / 3.0,
                     "triangles");
  }
  const auto drapedOver =
      [&drawnGround, &facesAt, &inFrame, &laid](double eastM, double southM, double fallback) {
        for (size_t rung = 0; rung < kDrapeRungs; ++rung) {
          const double cellM = DrapeCellM(rung);
          const auto cellE = static_cast<int64_t>(std::floor(eastM / cellM));
          const auto cellS = static_cast<int64_t>(std::floor(southM / cellM));
          const auto atE = static_cast<uint64_t>(cellE + 0x20000000LL);
          const auto atS = static_cast<uint64_t>(cellS + 0x20000000LL);
          const auto bucket = facesAt[rung].find((atE << 32U) | atS);
          if (bucket != facesAt[rung].end()) {
            for (const uint32_t at : bucket->second) {
              const size_t a = static_cast<size_t>(laid->Index[at]) * 3u;
              const size_t b = static_cast<size_t>(laid->Index[at + 1u]) * 3u;
              const size_t c = static_cast<size_t>(laid->Index[at + 2u]) * 3u;
              if (c + 2 >= inFrame.size()) { continue; }
              const double aE = inFrame[a];
              const double aS = inFrame[a + 2];
              const double spanBE = static_cast<double>(inFrame[b]) - aE;
              const double spanBS = static_cast<double>(inFrame[b + 2]) - aS;
              const double spanCE = static_cast<double>(inFrame[c]) - aE;
              const double spanCS = static_cast<double>(inFrame[c + 2]) - aS;
              const double twice = spanBE * spanCS - spanCE * spanBS;
              if (std::fabs(twice) < kLeastTurnRad) { continue; }
              const double intoE = eastM - aE;
              const double intoS = southM - aS;
              const double towardB = (intoE * spanCS - spanCE * intoS) / twice;
              const double towardC = (spanBE * intoS - intoE * spanBS) / twice;
              if (towardB < -kLeastRunM || towardC < -kLeastRunM ||
                  towardB + towardC > 1.0 + kLeastRunM) {
                continue;
              }
              return static_cast<double>(inFrame[a + 1]) * (1.0 - towardB - towardC) +
                     static_cast<double>(inFrame[b + 1]) * towardB +
                     static_cast<double>(inFrame[c + 1]) * towardC;
            }
          }
        }
        const double atE = eastM / kDrapeGridM;
        const double atS = southM / kDrapeGridM;
        const auto west = static_cast<int64_t>(std::floor(atE));
        const auto north = static_cast<int64_t>(std::floor(atS));
        const double alongE = atE - static_cast<double>(west);
        const double alongS = atS - static_cast<double>(north);
        const auto held = [&drawnGround](int64_t east, int64_t south, double *out) {
          const auto atE = static_cast<uint64_t>(east + 0x20000000LL);
          const auto atS = static_cast<uint64_t>(south + 0x20000000LL);
          const uint64_t key = (atE << 32U) | atS;
          const auto stood = drawnGround.find(key);
          if (stood == drawnGround.end()) { return false; }
          *out = static_cast<double>(stood->second);
          return true;
        };
        std::array<double, 4> corner = {{0.0, 0.0, 0.0, 0.0}};
        if (held(west, north, corner.data()) && held(west + 1, north, &corner[1]) &&
            held(west, north + 1, &corner[2]) && held(west + 1, north + 1, &corner[3])) {
          const double above = corner[0] + (corner[1] - corner[0]) * alongE;
          const double below = corner[2] + (corner[3] - corner[2]) * alongE;
          return above + (below - above) * alongS;
        }
        const auto east = static_cast<int64_t>(std::llround(atE));
        const auto south = static_cast<int64_t>(std::llround(atS));
        double summed = 0.0;
        size_t took = 0;
        for (int64_t dy = -1; dy <= 1; ++dy) {
          for (int64_t dx = -1; dx <= 1; ++dx) {
            double stood = 0.0;
            if (!held(east + dx, south + dy, &stood)) { continue; }
            summed += stood;
            ++took;
          }
        }
        return took > 0 ? summed / static_cast<double>(took) : fallback;
      };

  {
    Published.Places(
        "rebuild: of that, the drape the buildings stand on",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - censusAt)
            .count(),
        "ms");
    wiresAt = std::chrono::steady_clock::now();
    const Ground::StreetField &ways = World.Stack.Ways();
    const Ground::OsmField *const vectors = World.Stack.Vectors();
    std::vector<Generators::RoadStation> along;
    std::vector<Generators::RoadStation> finer;
    size_t chordAdded = 0;
    const int waterRow = World.Stack.Materials().Find("water");
    size_t decksOverWater = 0;
    size_t askedOverBridge = 0;
    size_t namedOverBridge = 0;
    size_t wetOverBridge = 0;
    double mostOverWaterM = 0.0;
    size_t laidWays = 0;
    size_t groundWays = 0;
    size_t refusedWays = 0;
    size_t fitLaid = 0;
    size_t fitRefused = 0;
    size_t fitUndrivable = 0;
    size_t fitTooTight = 0;
    std::vector<double> tightDemandM;
    size_t fitUnsplittable = 0;
    size_t fitCuts = 0;
    size_t sweptPieces = 0;
    size_t sweptCuts = 0;
    size_t sweptRefused = 0;
    Generators::RoadRefusals sweptWhy;
    std::vector<double> fitOffsetM;
    std::vector<double> fitRadiusM;
    std::vector<double> fitEastNorth;

    std::vector<double> deckM(ways.Ways().size(), -kBeyondAnyCoordinate);
    size_t crossingsSeen = 0;

    struct Meets {
      double EastM, SouthM;
      uint64_t Named;
    };

    std::unordered_map<uint64_t, std::vector<Meets>> atCrossing;
    size_t decksRaised = 0;
    double mostRaisedM = 0.0;
    if (vectors != nullptr) {
      const std::span<const double> points = vectors->Points();
      Path::Network net(kNodeSnapM, Data::kWgs84A);
      std::vector<size_t> netToLane;
      netToLane.reserve(ways.Ways().size());
      for (size_t at = 0; at < ways.Ways().size(); ++at) {
        const Ground::StreetField::Way &lane = ways.Ways()[at];
        if (lane.Form != Ground::StreetField::Shape::Ribbon || lane.PointCount < 2) { continue; }
        const size_t first = static_cast<size_t>(lane.FirstPoint) * 2;
        if (first + static_cast<size_t>(lane.PointCount) * 2 > points.size()) { continue; }
        net.Lay(points.subspan(first, static_cast<size_t>(lane.PointCount) * 2),
                Path::WayClass{.HalfWidthM = static_cast<double>(lane.HalfWidthM),
                               .MaxGradient = 0.0,
                               .MinRadiusM = 0.0,
                               .Friction = 0.0,
                               .Lanes = lane.Lanes,
                               .Spans = lane.Bridge});
        netToLane.push_back(at);
      }
      std::vector<Path::Network::Crossing> crossed;
      if (net.Crossings(crossed)) {
        crossingsSeen = crossed.size();
        for (const Path::Network::Crossing &one : crossed) {
          double eastM = 0.0;
          double upM = 0.0;
          double northM = 0.0;
          standing.Place(one.LatitudeDeg, one.LongitudeDeg, 0.0, &eastM, &upM, &northM);
          const uint64_t named = WayEndKey(one.LatitudeDeg, one.LongitudeDeg) | 1ULL;
          const auto east = static_cast<int64_t>(std::floor(eastM / kCrossCellM));
          const auto south = static_cast<int64_t>(std::floor(-northM / kCrossCellM));
          for (int64_t stepE = -1; stepE <= 1; ++stepE) {
            for (int64_t stepS = -1; stepS <= 1; ++stepS) {
              const auto atE = static_cast<uint64_t>(east + stepE + 0x20000000LL);
              const auto atS = static_cast<uint64_t>(south + stepS + 0x20000000LL);
              atCrossing[(atE << 32U) | atS].push_back(
                  Meets{.EastM = eastM, .SouthM = -northM, .Named = named});
            }
          }
        }
        for (const Path::Network::Crossing &one : crossed) {
          if (one.OverWay >= netToLane.size() || one.UnderWay >= netToLane.size()) { continue; }
          const size_t a = netToLane[one.OverWay];
          const size_t b = netToLane[one.UnderWay];
          const Ground::StreetField::Way &first = ways.Ways()[a];
          const Ground::StreetField::Way &second = ways.Ways()[b];
          if (first.Bridge == second.Bridge) { continue; }
          const size_t spans = first.Bridge ? a : b;
          const Ground::StreetField::Way &below = first.Bridge ? second : first;
          const std::optional<double> stood =
              World.Stack.Ground().At(one.LatitudeDeg, one.LongitudeDeg).AslM();
          if (!stood) { continue; }
          const double aslM = *stood;
          double eastM = 0.0;
          double upM = 0.0;
          double northM = 0.0;
          standing.Place(one.LatitudeDeg, one.LongitudeDeg, aslM, &eastM, &upM, &northM);
          const double onDrawn = drapedOver(eastM, -northM, upM);
          const double need = onDrawn + static_cast<double>(below.ClearanceM);
          if (need > deckM[spans]) {
            if (deckM[spans] < kUnraisedDeckM) { ++decksRaised; }
            deckM[spans] = need;
            mostRaisedM = std::max(mostRaisedM, need - onDrawn);
          }
        }
      }
    }
    std::unordered_map<uint64_t, double> endM;
    std::unordered_map<uint64_t, double> groundEndM;
    size_t rampsRaised = 0;
    double steepestRamp = 0.0;
    if (vectors != nullptr && decksRaised > 0) {
      const std::span<const double> points = vectors->Points();
      const auto endsOf = [&](const Ground::StreetField::Way &lane,
                              std::span<uint64_t, 2> out,
                              std::span<double, 4> at) {
        const size_t first = static_cast<size_t>(lane.FirstPoint) * 2u;
        const size_t last = first + (static_cast<size_t>(lane.PointCount) - 1u) * 2u;
        if (last + 1 >= points.size()) { return false; }
        at[0] = points[first];
        at[1] = points[first + 1];
        at[2] = points[last];
        at[3] = points[last + 1];
        out[0] = WayEndKey(at[0], at[1]);
        out[1] = WayEndKey(at[2], at[3]);
        return true;
      };
      const auto groundAt = [&](double lat, double lon, double *out) {
        const std::optional<double> stood = World.Stack.Ground().At(lat, lon).AslM();
        if (!stood) { return false; }
        const double aslM = *stood;
        double eastM = 0.0;
        double upM = 0.0;
        double northM = 0.0;
        standing.Place(lat, lon, aslM, &eastM, &upM, &northM);
        *out = drapedOver(eastM, -northM, upM);
        return true;
      };
      for (size_t at = 0; at < ways.Ways().size(); ++at) {
        const Ground::StreetField::Way &lane = ways.Ways()[at];
        if (lane.Form != Ground::StreetField::Shape::Ribbon || lane.PointCount < 2) { continue; }
        std::array<uint64_t, 2> key = {{0, 0}};
        std::array<double, 4> corner = {{0.0, 0.0, 0.0, 0.0}};
        if (!endsOf(lane, key, corner)) { continue; }
        for (int side = 0; side < 2; ++side) {
          double stood = 0.0;
          const size_t axis = static_cast<size_t>(side) * 2u;
          if (!groundAt(corner[axis], corner[axis + 1u], &stood)) { continue; }
          const auto found = endM.find(key[side]);
          if (found == endM.end()) {
            endM.emplace(key[side], stood);
            groundEndM.emplace(key[side], stood);
          } else {
            found->second = std::max(found->second, stood);
            const auto seeded = groundEndM.find(key[side]);
            if (seeded != groundEndM.end()) { seeded->second = std::max(seeded->second, stood); }
          }
        }
        if (lane.Bridge && deckM[at] > kUnraisedDeckM) {
          for (const uint64_t one : key) {
            const auto found = endM.find(one);
            if (found == endM.end()) {
              endM.emplace(one, deckM[at]);
            } else {
              found->second = std::max(found->second, deckM[at]);
            }
          }
        }
      }
      double mostDeckM = 0.0;
      for (const auto &one : endM) {
        const auto seeded = groundEndM.find(one.first);
        if (seeded == groundEndM.end()) { continue; }
        mostDeckM = std::max(mostDeckM, one.second - seeded->second);
      }
      Published.Places("streets: the highest deck a ramp must reach", mostDeckM, "m");
      for (int pass = 0; pass < kRampPasses; ++pass) {
        for (const Ground::StreetField::Way &lane : ways.Ways()) {
          if (lane.Form != Ground::StreetField::Shape::Ribbon || lane.PointCount < 2) { continue; }
          if (!(lane.MaxGradient > 0.0f)) { continue; }
          std::array<uint64_t, 2> key = {{0, 0}};
          std::array<double, 4> corner = {{0.0, 0.0, 0.0, 0.0}};
          if (!endsOf(lane, key, corner)) { continue; }
          const auto low = endM.find(key[0]);
          const auto high = endM.find(key[1]);
          if (low == endM.end() || high == endM.end()) { continue; }
          const double perLon = 111320.0 * std::cos(corner[0] * kDeg2Rad);
          const double runE = (corner[3] - corner[1]) * perLon;
          const double runN = (corner[2] - corner[0]) * kMPerDegLat;
          const double runM = std::sqrt(runE * runE + runN * runN);
          const double mostM = runM * static_cast<double>(lane.MaxGradient);
          const double apartM = high->second - low->second;
          const auto capped = [&](uint64_t at, double toM) {
            const auto seeded = groundEndM.find(at);
            return seeded == groundEndM.end() ? toM : std::min(toM, seeded->second + mostDeckM);
          };
          if (apartM > mostM) {
            low->second = capped(low->first, high->second - mostM);
          } else if (-apartM > mostM) {
            high->second = capped(high->first, low->second - mostM);
          }
        }
      }
      for (const Ground::StreetField::Way &lane : ways.Ways()) {
        if (lane.Bridge || lane.Form != Ground::StreetField::Shape::Ribbon) { continue; }
        std::array<uint64_t, 2> key = {{0, 0}};
        std::array<double, 4> corner = {{0.0, 0.0, 0.0, 0.0}};
        if (lane.PointCount < 2 || !endsOf(lane, key, corner)) { continue; }
        Vec2 stood = {{0.0, 0.0}};
        if (!groundAt(corner[0], corner[1], stood.data()) ||
            !groundAt(corner[2], corner[3], &stood[1])) {
          continue;
        }
        double rose = 0.0;
        for (int side = 0; side < 2; ++side) {
          const auto found = endM.find(key[side]);
          if (found == endM.end()) { continue; }
          rose = std::max(rose, found->second - stood[side]);
        }
        if (rose > kRoseLeast) {
          ++rampsRaised;
          steepestRamp = std::max(steepestRamp, rose);
        }
      }
    }
    Published.Places(
        "streets: ways a ramp lifted off the ground", static_cast<double>(rampsRaised), "ways");
    Published.Places("streets: and the most one was lifted", steepestRamp, "m");
    Published.Places(
        "streets: crossings the plan found", static_cast<double>(crossingsSeen), "crossings");
    Published.Places("streets: decks a crossing raised", static_cast<double>(decksRaised), "decks");
    Published.Places("streets: and the most one stands over what it crosses", mostRaisedM, "m");
    std::unordered_map<uint64_t, std::vector<Generators::RoadGate>> gates;
    std::vector<double> trimM(ways.Ways().size() * 2u, 0.0);
    size_t endsTrimmed = 0;
    size_t endsStillCrossing = 0;
    double deepestTrimM = 0.0;
    std::vector<double> shortByM;
    std::vector<double> forkDeg;
    if (vectors != nullptr) {
      const std::span<const double> points = vectors->Points();

      struct Leaving {
        uint32_t Way = 0;
        uint8_t Side = 0;
        float DirE = 0.0f;
        float DirN = 0.0f;
        float HalfM = 0.0f;
      };

      std::unordered_map<uint64_t, std::vector<Leaving>> meeting;
      for (size_t at = 0; at < ways.Ways().size(); ++at) {
        const Ground::StreetField::Way &lane = ways.Ways()[at];
        if (lane.Form != Ground::StreetField::Shape::Ribbon || lane.PointCount < 2) { continue; }
        if (!(lane.HalfWidthM > 0.0f)) { continue; }
        const size_t first = static_cast<size_t>(lane.FirstPoint) * 2u;
        const size_t last = first + (static_cast<size_t>(lane.PointCount) - 1u) * 2u;
        if (last + 1 >= points.size()) { continue; }
        for (int side = 0; side < 2; ++side) {
          const size_t here = side == 0 ? first : last;
          const size_t next = side == 0 ? first + 2u : last - 2u;
          const double perLon = 111320.0 * std::cos(points[here] * kDeg2Rad);
          double outE = (points[next + 1] - points[here + 1]) * perLon;
          double outN = (points[next] - points[here]) * kMPerDegLat;
          const double run = std::sqrt(outE * outE + outN * outN);
          if (!(run > kLeastRunM)) { continue; }
          outE /= run;
          outN /= run;
          meeting[WayEndKey(points[here], points[here + 1])].push_back(
              Leaving{.Way = static_cast<uint32_t>(at),
                      .Side = static_cast<uint8_t>(side),
                      .DirE = static_cast<float>(outE),
                      .DirN = static_cast<float>(outN),
                      .HalfM = lane.HalfWidthM});
        }
      }
      for (const auto &node : meeting) {
        const std::vector<Leaving> &leaving = node.second;
        if (leaving.size() < 2) { continue; }
        for (const Leaving &mine : leaving) {
          double back = 0.0;
          for (const Leaving &other : leaving) {
            if (other.Way == mine.Way && other.Side == mine.Side) { continue; }
            const double cosBetween = static_cast<double>(mine.DirE) * other.DirE +
                                      static_cast<double>(mine.DirN) * other.DirN;
            const double sinBetween = std::fabs(static_cast<double>(mine.DirE) * other.DirN -
                                                static_cast<double>(mine.DirN) * other.DirE);
            if (sinBetween < kLeastSineBetween) { continue; }
            const double reach =
                (static_cast<double>(other.HalfM) + static_cast<double>(mine.HalfM) * cosBetween) /
                sinBetween;
            back = std::max(back, reach);
          }
          double sharpest = kDegPerHalfTurn;
          for (const Leaving &other : leaving) {
            if (other.Way == mine.Way && other.Side == mine.Side) { continue; }
            const double between =
                std::acos(std::clamp(static_cast<double>(mine.DirE) * other.DirE +
                                         static_cast<double>(mine.DirN) * other.DirN,
                                     -1.0,
                                     1.0));
            sharpest = std::min(sharpest, kDegPerHalfTurn - between * kRad2Deg);
          }
          const double capped = std::min(back, static_cast<double>(mine.HalfM) * kTrimMostWidths);
          trimM[static_cast<size_t>(mine.Way) * 2u + mine.Side] = capped;
          if (capped > kLeastCapM) {
            ++endsTrimmed;
            deepestTrimM = std::max(deepestTrimM, capped);
          }
          if (back > capped + kLeastCapM) {
            ++endsStillCrossing;
            shortByM.push_back(back - capped);
            forkDeg.push_back(sharpest);
          }
        }
      }
    }
    if (!shortByM.empty()) {
      std::ranges::sort(shortByM);
      std::ranges::sort(forkDeg);
      const auto pick = [](const std::vector<double> &of, double part) {
        return of[static_cast<size_t>(static_cast<double>(of.size() - 1u) * part)];
      };
      Published.Places("streets: what a capped end was short by, p50", pick(shortByM, 0.5), "m");
      Published.Places("streets: and p95", pick(shortByM, kBroadQuantile), "m");
      Published.Places("streets: and the most", shortByM.back(), "m");
      Published.Places("streets: the fork angle where the cap bit, p50", pick(forkDeg, 0.5), "deg");
      Published.Places("streets: and the sharpest", forkDeg.front(), "deg");
    }
    Published.Places(
        "streets: way ends a junction trimmed", static_cast<double>(endsTrimmed), "ends");
    Published.Places("streets: and the deepest trim", deepestTrimM, "m");
    Published.Places("streets: ends STILL crossing, the cap bit",
                     static_cast<double>(endsStillCrossing),
                     "ends");
    if (vectors != nullptr) {
      const std::span<const double> points = vectors->Points();
      std::unordered_map<uint64_t, uint32_t> sharedNodes;
      for (const Ground::StreetField::Way &one : ways.Ways()) {
        if (one.Form != Ground::StreetField::Shape::Ribbon) { continue; }
        for (uint32_t step = 0; step < one.PointCount; ++step) {
          const size_t at = (static_cast<size_t>(one.FirstPoint) + step) * 2u;
          if (at + 1 >= points.size()) { break; }
          ++sharedNodes[WayEndKey(points[at], points[at + 1])];
        }
      }
      std::vector<std::vector<Generators::RoadStation>> designed(ways.Ways().size());
      for (int phase = 0; phase < 2; ++phase) {
        for (size_t laneAt = 0; laneAt < ways.Ways().size(); ++laneAt) {
          const Ground::StreetField::Way &lane = ways.Ways()[laneAt];
          if (lane.Form != Ground::StreetField::Shape::Ribbon || lane.PointCount < 2 ||
              !(lane.HalfWidthM > 0.0f)) {
            refusedWays += phase == 0 ? 1u : 0u;
            continue;
          }
          if (phase == 1) {
            along = designed[laneAt];
            if (along.size() < 2) { continue; }
          } else {
            along.clear();
            bool whole = true;
            const auto station = [&](double lat, double lon, uint64_t node) {
              const std::optional<double> stood = World.Stack.Ground().At(lat, lon).AslM();
              if (!stood) { return false; }
              const double aslM = *stood;
              double eastM = 0.0;
              double upM = 0.0;
              double northM = 0.0;
              standing.Place(lat, lon, aslM, &eastM, &upM, &northM);
              along.push_back({.EastM = eastM,
                               .SouthM = -northM,
                               .GradeM = drapedOver(eastM, -northM, upM),
                               .Node = node});
              return true;
            };
            for (uint32_t step = 0; step + 1 < lane.PointCount && whole; ++step) {
              const size_t here = (static_cast<size_t>(lane.FirstPoint) + step) * 2;
              const size_t next = here + 2;
              if (next + 1 >= points.size()) {
                whole = false;
                break;
              }
              const double perLon = 111320.0 * std::cos(points[here] * kDeg2Rad);
              const double spanE = (points[next + 1] - points[here + 1]) * perLon;
              const double spanN = (points[next] - points[here]) * kMPerDegLat;
              const auto pieces =
                  static_cast<size_t>(1.0 + std::sqrt(spanE * spanE + spanN * spanN) / kRoadStepM);
              for (size_t piece = 0; piece < pieces && whole; ++piece) {
                const double at = static_cast<double>(piece) / static_cast<double>(pieces);
                const double onLat = points[here] + (points[next] - points[here]) * at;
                const double onLon = points[here + 1] + (points[next + 1] - points[here + 1]) * at;
                const auto seen =
                    piece == 0 ? sharedNodes.find(WayEndKey(onLat, onLon)) : sharedNodes.end();
                whole = station(
                    onLat,
                    onLon,
                    seen != sharedNodes.end() && seen->second > 1u ? WayEndKey(onLat, onLon) : 0u);
              }
            }
            if (whole) {
              const size_t last = (static_cast<size_t>(lane.FirstPoint) + lane.PointCount - 1u) * 2;
              if (last + 1 < points.size()) {
                const auto seen = sharedNodes.find(WayEndKey(points[last], points[last + 1]));
                whole = station(points[last],
                                points[last + 1],
                                seen != sharedNodes.end() && seen->second > 1u
                                    ? WayEndKey(points[last], points[last + 1])
                                    : 0ULL);
              } else {
                whole = false;
              }
            }
            if (!whole || along.size() < 2) {
              ++refusedWays;
              continue;
            }
            for (int pass = 0; pass < kChordPasses; ++pass) {
              size_t added = 0;
              finer.clear();
              finer.reserve(along.size() * 2u);
              for (size_t at = 1; at < along.size(); ++at) {
                finer.push_back(along[at - 1u]);
                const double midE = 0.5 * (along[at - 1u].EastM + along[at].EastM);
                const double midS = 0.5 * (along[at - 1u].SouthM + along[at].SouthM);
                const double chord = 0.5 * (along[at - 1u].GradeM + along[at].GradeM);
                const double overM = drapedOver(midE, midS, chord);
                if (std::fabs(overM - chord) <= kChordWithinM) { continue; }
                finer.push_back(
                    Generators::RoadStation{.EastM = midE, .SouthM = midS, .GradeM = overM});
                ++added;
              }
              finer.push_back(along.back());
              along.swap(finer);
              chordAdded += added;
              if (added == 0) { break; }
            }

            if (lane.MaxGradient > 0.0f) {
              Generators::DesignProfile(Span<Generators::RoadStation>(along.data(), along.size()),
                                        static_cast<double>(lane.MaxGradient),
                                        kLeastCrestK);
            }
            for (Generators::RoadStation &one : along) {
              if (one.Node != 0u || atCrossing.empty()) { continue; }
              const auto east = static_cast<int64_t>(std::floor(one.EastM / kCrossCellM));
              const auto south = static_cast<int64_t>(std::floor(one.SouthM / kCrossCellM));
              const auto atE = static_cast<uint64_t>(east + 0x20000000LL);
              const auto atS = static_cast<uint64_t>(south + 0x20000000LL);
              const auto near = atCrossing.find((atE << 32U) | atS);
              if (near == atCrossing.end()) { continue; }
              for (const auto &met : near->second) {
                const double offE = one.EastM - met.EastM;
                const double offS = one.SouthM - met.SouthM;
                if (offE * offE + offS * offS <= kMeetsWithinM * kMeetsWithinM) {
                  one.Node = met.Named;
                  break;
                }
              }
            }
            designed[laneAt] = along;
            continue;
          }

          {
            fitEastNorth.clear();
            fitEastNorth.reserve(along.size() * 2u);
            for (const Generators::RoadStation &one : along) {
              fitEastNorth.push_back(one.EastM);
              fitEastNorth.push_back(-one.SouthM);
            }
            size_t from = 0;
            size_t cuts = 0;
            bool wholeWay = true;
            while (from * 2u + 6u <= fitEastNorth.size()) {
              ReferenceLine fitted;
              const Fitted got = Fit(Span<const double>(fitEastNorth.data() + from * 2u,
                                                        fitEastNorth.size() - from * 2u),
                                     kFitWithinM,
                                     kFitTightestM,
                                     fitted);
              if (got.Laid) {
                ++fitLaid;
                fitOffsetM.push_back(got.WorstOffsetM);
                if (got.TightestRadiusM > 0.0) { fitRadiusM.push_back(got.TightestRadiusM); }
                fitUndrivable += got.Undrivable;
                break;
              }
              if (got.Undrivable == 0 || got.TightestDemandedAtVertex == 0) {
                if (wholeWay) { ++fitRefused; }
                ++fitUnsplittable;
                break;
              }
              const size_t upTo = got.TightestDemandedAtVertex;
              if (upTo + 1u >= 3u) {
                ReferenceLine piece;
                const Fitted head =
                    Fit(Span<const double>(fitEastNorth.data() + from * 2u, (upTo + 1u) * 2u),
                        kFitWithinM,
                        kFitTightestM,
                        piece);
                if (head.Laid) {
                  ++fitLaid;
                  fitOffsetM.push_back(head.WorstOffsetM);
                  if (head.TightestRadiusM > 0.0) { fitRadiusM.push_back(head.TightestRadiusM); }
                } else {
                  ++fitUnsplittable;
                }
              }
              ++fitTooTight;
              ++cuts;
              if (got.TightestDemandedM > 0.0) { tightDemandM.push_back(got.TightestDemandedM); }
              from += upTo + 1u;
              wholeWay = false;
            }
            fitCuts += cuts;
          }

          {
            std::vector<double> reached(along.size(), 0.0);
            for (size_t at = 1; at < along.size(); ++at) {
              const double spanE = along[at].EastM - along[at - 1].EastM;
              const double spanS = along[at].SouthM - along[at - 1].SouthM;
              reached[at] = reached[at - 1] + std::sqrt(spanE * spanE + spanS * spanS);
            }
            const double wholeM = reached.back();
            const double fromM = trimM[laneAt * 2u];
            const double toM = wholeM - trimM[laneAt * 2u + 1u];
            if (toM - fromM >= kLeastRoadM && (fromM > kLeastCapM || toM < wholeM - kLeastCapM)) {
              const auto standAt = [&](double alongM) {
                size_t at = 1;
                while (at + 1 < reached.size() && reached[at] < alongM) { ++at; }
                const double span = reached[at] - reached[at - 1];
                const double part = span > kLeastTurnRad ? (alongM - reached[at - 1]) / span : 0.0;
                const Generators::RoadStation &from = along[at - 1];
                const Generators::RoadStation &to = along[at];
                return Generators::RoadStation{
                    .EastM = from.EastM + (to.EastM - from.EastM) * part,
                    .SouthM = from.SouthM + (to.SouthM - from.SouthM) * part,
                    .GradeM = from.GradeM + (to.GradeM - from.GradeM) * part};
              };
              std::vector<Generators::RoadStation> kept;
              kept.push_back(standAt(fromM));
              for (size_t at = 0; at < along.size(); ++at) {
                if (reached[at] > fromM && reached[at] < toM) { kept.push_back(along[at]); }
              }
              kept.push_back(standAt(toM));
              along.swap(kept);
            }
          }
          if (along.size() < 2) {
            ++refusedWays;
            continue;
          }
          if (lane.Bridge && waterRow >= 0 && classStructure) {
            double overWaterM = 0.0;
            for (size_t at = 1; at < along.size(); ++at) {
              double lat = 0.0;
              double lon = 0.0;
              const double midE = 0.5 * (along[at - 1].EastM + along[at].EastM);
              const double midS = 0.5 * (along[at - 1].SouthM + along[at].SouthM);
              standing.Geo(midE, -midS, &lat, &lon);
              double edgeM = 0.0;
              int second = -1;
              const int which =
                  World.Stack.Classes().ClassAt(*classStructure, lat, lon, &edgeM, &second);
              ++askedOverBridge;
              if (which < 0 ||
                  static_cast<size_t>(which) >= World.Stack.Vegetation().TemplateCount()) {
                continue;
              }
              ++namedOverBridge;
              if (World.Stack.Vegetation().Rows()[static_cast<size_t>(which)].GroundClass !=
                  waterRow) {
                continue;
              }
              ++wetOverBridge;
              const double spanE = along[at].EastM - along[at - 1].EastM;
              const double spanS = along[at].SouthM - along[at - 1].SouthM;
              overWaterM += std::sqrt(spanE * spanE + spanS * spanS);
            }
            if (overWaterM > 0.0) {
              double clear = 0.0;
              for (const Ground::VegetationTemplates::WaterBand &band :
                   World.Stack.Vegetation().WaterBands()) {
                clear = static_cast<double>(band.ClearanceM);
                if (overWaterM <= static_cast<double>(band.RunM)) { break; }
              }
              if (clear > 0.0) {
                double stood = -kBeyondAnyCoordinate;
                for (const Generators::RoadStation &one : along) {
                  stood = std::max(stood, one.GradeM);
                }
                deckM[laneAt] = std::max(deckM[laneAt], stood + clear);
                ++decksOverWater;
                mostOverWaterM = std::max(mostOverWaterM, clear);
              }
            }
          }
          if (lane.Bridge) {
            double deck = deckM[laneAt];
            for (const Generators::RoadStation &one : along) { deck = std::max(deck, one.GradeM); }
            for (Generators::RoadStation &one : along) { one.GradeM = deck; }
          } else if (!endM.empty()) {
            const size_t first = static_cast<size_t>(lane.FirstPoint) * 2;
            const size_t last = first + (static_cast<size_t>(lane.PointCount) - 1u) * 2;
            if (last + 1 < points.size()) {
              const auto from = endM.find(WayEndKey(points[first], points[first + 1]));
              const auto to = endM.find(WayEndKey(points[last], points[last + 1]));
              if (from != endM.end() && to != endM.end()) {
                double runM = 0.0;
                std::vector<double> reached(along.size(), 0.0);
                for (size_t at = 1; at < along.size(); ++at) {
                  const double spanE = along[at].EastM - along[at - 1].EastM;
                  const double spanS = along[at].SouthM - along[at - 1].SouthM;
                  runM += std::sqrt(spanE * spanE + spanS * spanS);
                  reached[at] = runM;
                }
                for (size_t at = 0; at < along.size(); ++at) {
                  const double along01 = runM > kLeastRunM ? reached[at] / runM : 0.0;
                  const double wanted = from->second + (to->second - from->second) * along01;
                  along[at].GradeM = std::max(along[at].GradeM, wanted);
                }
              }
            }
          }
          laidWays += lane.Bridge ? 1u : 0u;
          groundWays += lane.Bridge ? 0u : 1u;
          const bool sealed =
              lane.CoverRow >= 0 &&
              static_cast<size_t>(lane.CoverRow) < World.Stack.Vegetation().TemplateCount() &&
              World.Stack.Vegetation().Rows()[static_cast<size_t>(lane.CoverRow)].Mix[2] >= 1.0f;
          Generators::RoadProfile profile = Generators::RoadProfile::Rounded;
          if (sealed) {
            profile =
                lane.Lanes >= 2 ? Generators::RoadProfile::Kerbed : Generators::RoadProfile::Simple;
          }
          Vec3f wears = {{0.5f, 0.5f, 0.5f}};
          if (lane.CoverRow >= 0 &&
              static_cast<size_t>(lane.CoverRow) < World.Stack.Vegetation().TemplateCount()) {
            const Vec4f &cover =
                World.Stack.Vegetation().Rows()[static_cast<size_t>(lane.CoverRow)].Ground;
            wears = {{cover[0], cover[1], cover[2]}};
          }
          if (lane.Bridge) {
            Generators::SweepRoad(Span<const Generators::RoadStation>(along.data(), along.size()),
                                  static_cast<double>(lane.HalfWidthM),
                                  profile,
                                  wears,
                                  std::atan(Generators::kCrossfall),
                                  pavement,
                                  &sweptPieces,
                                  &sweptCuts,
                                  &sweptRefused,
                                  &sweptWhy);
          }
          for (size_t at = 1; at < along.size(); ++at) {
            const double runE = along[at].EastM - along[at - 1u].EastM;
            const double runS = along[at].SouthM - along[at - 1u].SouthM;
            const double runM = std::sqrt(runE * runE + runS * runS);
            if (!(runM > kLeastSpanM)) { continue; }
            const double groundAt =
                drapedOver(along[at].EastM, -along[at].SouthM, along[at].GradeM);
            const double groundBefore =
                drapedOver(along[at - 1u].EastM, -along[at - 1u].SouthM, along[at - 1u].GradeM);
            const double yieldM = std::max(std::fabs(along[at].GradeM - groundAt),
                                           std::fabs(along[at - 1u].GradeM - groundBefore));
            const double outE = -runS / runM;
            const double outS = runE / runM;
            const double half = static_cast<double>(lane.HalfWidthM) + kVergeM;
            double reliefM = std::fabs(groundAt - groundBefore);
            for (const double hand : {1.0, -1.0}) {
              for (int end = 0; end < 2; ++end) {
                const Generators::RoadStation &one = end == 0 ? along[at - 1u] : along[at];
                const double sideE = one.EastM + outE * half * hand;
                const double sideS = one.SouthM + outS * half * hand;
                reliefM = std::max(reliefM, drapedOver(sideE, -sideS, one.GradeM) - one.GradeM);
              }
            }
            if (yieldM < kStampWorthM && reliefM < kBrokenGroundM) { continue; }
            Yields made;
            made.RingEastSouthM = {along[at - 1u].EastM + outE * half,
                                   along[at - 1u].SouthM + outS * half,
                                   along[at].EastM + outE * half,
                                   along[at].SouthM + outS * half,
                                   along[at].EastM - outE * half,
                                   along[at].SouthM - outS * half,
                                   along[at - 1u].EastM - outE * half,
                                   along[at - 1u].SouthM - outS * half};
            made.LowE = made.HighE = made.RingEastSouthM[0];
            made.LowS = made.HighS = made.RingEastSouthM[1];
            for (size_t k = 2; k + 1 < made.RingEastSouthM.size(); k += 2) {
              made.LowE = std::min(made.LowE, made.RingEastSouthM[k]);
              made.HighE = std::max(made.HighE, made.RingEastSouthM[k]);
              made.LowS = std::min(made.LowS, made.RingEastSouthM[k + 1]);
              made.HighS = std::max(made.HighS, made.RingEastSouthM[k + 1]);
            }
            made.AtE = along[at - 1u].EastM;
            made.AtS = along[at - 1u].SouthM;
            made.PlateauM = along[at - 1u].GradeM;
            const double rise = (along[at].GradeM - along[at - 1u].GradeM) / runM;
            made.SlopeE = rise * runE / runM;
            made.SlopeS = rise * runS / runM;
            made.ApronM = std::clamp(kBatterRun * yieldM, kLeastApronM, kMostApronM);
            made.YieldM = yieldM;
            const bool rests = !lane.Bridge || at == 1u || at + 1u == along.size();
            if (rests) {
              made.SeamEastSouthM = {
                  along[at - 1u].EastM + outE * static_cast<double>(lane.HalfWidthM),
                  along[at - 1u].SouthM + outS * static_cast<double>(lane.HalfWidthM),
                  along[at].EastM + outE * static_cast<double>(lane.HalfWidthM),
                  along[at].SouthM + outS * static_cast<double>(lane.HalfWidthM),
                  along[at].EastM - outE * static_cast<double>(lane.HalfWidthM),
                  along[at].SouthM - outS * static_cast<double>(lane.HalfWidthM),
                  along[at - 1u].EastM - outE * static_cast<double>(lane.HalfWidthM),
                  along[at - 1u].SouthM - outS * static_cast<double>(lane.HalfWidthM)};
            }
            made.Fills = !lane.Bridge;
            corridor.push_back(std::move(made));
          }
          if (!lane.Bridge && along.size() > 3) {
            const size_t shutFrom = static_cast<size_t>(lane.FirstPoint) * 2u;
            const size_t shutTo = shutFrom + (static_cast<size_t>(lane.PointCount) - 1u) * 2u;
            const bool shut = shutTo + 1 < points.size() &&
                              std::fabs(points[shutFrom] - points[shutTo]) < 1.0e-7 &&
                              std::fabs(points[shutFrom + 1] - points[shutTo + 1]) < 1.0e-7;
            if (shut) {
              Yields island;
              island.RingEastSouthM.reserve(along.size() * 2u);
              island.LowE = island.HighE = along.front().EastM;
              island.LowS = island.HighS = along.front().SouthM;
              double summed = 0.0;
              for (const Generators::RoadStation &one : along) {
                island.RingEastSouthM.push_back(one.EastM);
                island.RingEastSouthM.push_back(one.SouthM);
                island.LowE = std::min(island.LowE, one.EastM);
                island.HighE = std::max(island.HighE, one.EastM);
                island.LowS = std::min(island.LowS, one.SouthM);
                island.HighS = std::max(island.HighS, one.SouthM);
                summed += one.GradeM;
              }
              island.AtE = along.front().EastM;
              island.AtS = along.front().SouthM;
              island.PlateauM = summed / static_cast<double>(along.size());
              island.ApronM = kLeastApronM;
              island.YieldM = kBrokenGroundM;
              island.Fills = true;
              corridor.push_back(std::move(island));
            }
          }
          {
            const size_t first = static_cast<size_t>(lane.FirstPoint) * 2u;
            const size_t last = first + (static_cast<size_t>(lane.PointCount) - 1u) * 2u;
            if (last + 1 < points.size()) {
              const std::array<uint64_t, 2> key = {{WayEndKey(points[first], points[first + 1]),
                                                    WayEndKey(points[last], points[last + 1])}};
              for (int side = 0; side < 2; ++side) {
                const Generators::RoadStation &at = side == 0 ? along.front() : along.back();
                const Generators::RoadStation &to = side == 0 ? along[1] : along[along.size() - 2u];
                double outE = to.EastM - at.EastM;
                double outS = to.SouthM - at.SouthM;
                const double run = std::sqrt(outE * outE + outS * outS);
                if (!(run > kLeastRunM)) { continue; }
                outE /= run;
                outS /= run;
                gates[key[side]].push_back(
                    Generators::RoadGate{.EastM = at.EastM,
                                         .SouthM = at.SouthM,
                                         .GradeM = at.GradeM,
                                         .OutE = outE,
                                         .OutS = outS,
                                         .HalfWidthM = static_cast<double>(lane.HalfWidthM)});
              }
            }
          }
        }
        if (phase == 0) {
          double movedM = 0.0;
          for (int pass = 0; pass < kLevelPasses; ++pass) {
            std::unordered_map<uint64_t, std::vector<std::pair<uint32_t, uint32_t>>> atNode;
            for (uint32_t lane = 0; lane < static_cast<uint32_t>(designed.size()); ++lane) {
              for (uint32_t one = 0; one < static_cast<uint32_t>(designed[lane].size()); ++one) {
                if (designed[lane][one].Node == 0u) { continue; }
                atNode[designed[lane][one].Node].emplace_back(lane, one);
              }
            }
            std::vector<double> pullM(designed.size(), 0.0);
            std::vector<uint32_t> pulls(designed.size(), 0u);
            for (const auto &node : atNode) {
              if (node.second.size() < 2) { continue; }
              double wanted = 0.0;
              for (const auto &held : node.second) {
                wanted += designed[held.first][held.second].GradeM;
              }
              wanted /= static_cast<double>(node.second.size());
              for (const auto &held : node.second) {
                pullM[held.first] += wanted - designed[held.first][held.second].GradeM;
                ++pulls[held.first];
              }
            }
            double most = 0.0;
            for (size_t lane = 0; lane < designed.size(); ++lane) {
              if (pulls[lane] == 0u) { continue; }
              const double by = pullM[lane] / static_cast<double>(pulls[lane]);
              for (Generators::RoadStation &one : designed[lane]) { one.GradeM += by; }
              most = std::max(most, std::fabs(by));
            }
            movedM = most;
            if (most < kLevelledM) { break; }
          }
          Published.Places("streets: the levelling's last shift", movedM, "m");
        }
      }
    }
    Published.Places(
        "streets: stations under a bridge asked", static_cast<double>(askedOverBridge), "stations");
    Published.Places(
        "streets: of those a class named", static_cast<double>(namedOverBridge), "stations");
    Published.Places(
        "streets: and of those, water", static_cast<double>(wetOverBridge), "stations");
    Published.Places(
        "streets: the water class the table names", static_cast<double>(waterRow), "index");
    Published.Places("streets: a class structure stood", classStructure ? 1.0 : 0.0, "yes/no");
    Published.Places(
        "streets: decks a WATERWAY raised", static_cast<double>(decksOverWater), "decks");
    Published.Places("streets: and the clearance the widest one took", mostOverWaterM, "m");
    size_t junctionsRaised = 0;
    {
      std::vector<uint64_t> nodes;
      nodes.reserve(gates.size());
      for (const auto &one : gates) {
        if (one.second.size() >= 2) { nodes.push_back(one.first); }
      }
      std::ranges::sort(nodes);
      const int asphalt = World.Stack.Materials().Find("asphalt");
      Vec3f wears = {{0.5f, 0.5f, 0.5f}};
      if (asphalt >= 0) { wears = World.Stack.Materials().At(static_cast<size_t>(asphalt)).Albedo; }
      for (const uint64_t node : nodes) {
        const std::vector<Generators::RoadGate> &met = gates[node];
        Generators::RaiseJunction(
            Span<const Generators::RoadGate>(met.data(), met.size()), wears, pavement);
        ++junctionsRaised;
      }
    }
    Published.Places(
        "streets: junction bodies raised", static_cast<double>(junctionsRaised), "junctions");
    if (!fitOffsetM.empty()) {
      std::ranges::sort(fitOffsetM);
      std::ranges::sort(fitRadiusM);
      const auto pick = [](const std::vector<double> &of, double part) {
        return of.empty() ? 0.0
                          : of[static_cast<size_t>(static_cast<double>(of.size() - 1u) * part)];
      };
      Published.Places(
          "streets: ways a reference line was fitted to", static_cast<double>(fitLaid), "ways");
      Published.Places(
          "streets: and ways the fit refused", static_cast<double>(fitRefused), "ways");
      Published.Places("streets: corners too tight to drive, cut instead",
                       static_cast<double>(fitTooTight),
                       "corners");
      Published.Places("streets: cuts the split made", static_cast<double>(fitCuts), "cuts");
      Published.Places(
          "streets: stations a chord asked for", static_cast<double>(chordAdded), "stations");
      Published.Places(
          "streets: pieces the sweep laid on a line", static_cast<double>(sweptPieces), "pieces");
      Published.Places("streets: cuts the sweep made", static_cast<double>(sweptCuts), "cuts");
      Published.Places(
          "streets: pieces the sweep could not lay", static_cast<double>(sweptRefused), "pieces");
      Published.Places(
          "streets: of those, the fit refused", static_cast<double>(sweptWhy.Fit), "pieces");
      Published.Places(
          "streets: of those, the rise refused", static_cast<double>(sweptWhy.Rise), "pieces");
      Published.Places(
          "streets: of those, the bank refused", static_cast<double>(sweptWhy.Bank), "pieces");
      Published.Places(
          "streets: of those, the sweep refused", static_cast<double>(sweptWhy.Sweep), "pieces");
      Published.Places(
          "streets: of those, too short to lay", static_cast<double>(sweptWhy.TooShort), "pieces");
      Published.Places("streets: pieces the split still could not lay",
                       static_cast<double>(fitUnsplittable),
                       "pieces");
      if (!tightDemandM.empty()) {
        std::ranges::sort(tightDemandM);
        Published.Places("streets: the radius such a corner demanded, p50",
                         tightDemandM[tightDemandM.size() / 2u],
                         "m");
        Published.Places("streets: and the tightest", tightDemandM.front(), "m");
      }
      Published.Places("streets: the offset a fitted line needed, p50", pick(fitOffsetM, 0.5), "m");
      Published.Places(
          "streets: the offset a fitted line needed, p95", pick(fitOffsetM, kBroadQuantile), "m");
      Published.Places("streets: the offset a fitted line needed, worst", fitOffsetM.back(), "m");
      Published.Places(
          "streets: the radius a fitted line found, tightest", pick(fitRadiusM, 0.0), "m");
      Published.Places("streets: the radius a fitted line found, p50", pick(fitRadiusM, 0.5), "m");
      Published.Places("streets: stations the fit calls undrivable",
                       static_cast<double>(fitUndrivable),
                       "stations");
    }
    Published.Places("streets: ways laid as ribbons, all of them FLOATING",
                     static_cast<double>(laidWays),
                     "ways");
    Published.Places(
        "streets: ways the GROUND carries instead", static_cast<double>(groundWays), "ways");
    Published.Places(
        "streets: ways the field holds", static_cast<double>(ways.Ways().size()), "ways");
    Published.Places(
        "streets: features it walked at all", static_cast<double>(ways.LookedCount()), "features");
    Published.Places(
        "streets: features no rule named", static_cast<double>(ways.UnruledCount()), "features");
    Published.Places("streets: features a rule gave no width",
                     static_cast<double>(ways.UnwidthedCount()),
                     "features");
    Published.Places(
        "streets: features that are tunnels", static_cast<double>(ways.TunnelCount()), "features");
    Published.Places(
        "streets: ways OSM calls a bridge", static_cast<double>(ways.BridgeCount()), "ways");
    Published.Places(
        "streets: ways that state a layer", static_cast<double>(ways.LayeredCount()), "ways");
    Published.Places("streets: ways whose layer is a STRING",
                     static_cast<double>(ways.LayerSaidCount()),
                     "ways");
    Published.Places("streets: ways it refused", static_cast<double>(refusedWays), "ways");
    {
      std::unordered_map<uint64_t, uint32_t> corner;
      corner.reserve(pavement.PositionM.size() / 3);
      size_t shared = 0;
      for (size_t at = 0; at + 2 < pavement.PositionM.size(); at += 3) {
        uint64_t keyed = kDigestBasis;
        for (size_t axis = 0; axis < 3; ++axis) {
          keyed = (keyed ^ std::bit_cast<uint32_t>(pavement.PositionM[at + axis])) * kDigestPrime;
        }
        if (++corner[keyed] == 2u) { shared += 2; }
      }
      const size_t corners = pavement.PositionM.size() / 3u;
      Published.Places(
          "streets: vertices two bodies SHARE", static_cast<double>(shared), "vertices");
      Published.Places("streets: vertices in all", static_cast<double>(corners), "vertices");
    }
    {
      std::vector<double> aboveM;
      aboveM.reserve(pavement.PositionM.size() / 3u);
      size_t flying = 0;
      for (size_t vertex = 0; vertex + 2 < pavement.PositionM.size(); vertex += 3) {
        const double under = drapedOver(
            pavement.PositionM[vertex], pavement.PositionM[vertex + 2], -kBeyondAnyCoordinate);
        if (under < kUnraisedDeckM) { continue; }
        const double aloft = static_cast<double>(pavement.PositionM[vertex + 1]) - under;
        aboveM.push_back(aloft);
        if (aloft > kFlyingM) { ++flying; }
      }
      if (!aboveM.empty()) {
        std::ranges::sort(aboveM);
        const auto pick = [&aboveM](double part) {
          return aboveM[static_cast<size_t>(static_cast<double>(aboveM.size() - 1u) * part)];
        };
        Published.Places("streets: a vertex stands over the ground, p50", pick(0.5), "m");
        Published.Places(
            "streets: a vertex stands over the ground, p95", pick(kBroadQuantile), "m");
        Published.Places("streets: a vertex stands over the ground, highest", aboveM.back(), "m");
        Published.Places("streets: a vertex stands under it, deepest", aboveM.front(), "m");
        Published.Places(
            "streets: vertices FLYING, over the bar", static_cast<double>(flying), "vertices");
      }
    }
    Published.Places(
        "streets: triangles", static_cast<double>(pavement.Index.size() / 3), "triangles");
    if (pavement.Index.size() >= 3) {
      Material tarmac;
      for (int channel = 0; channel < 3; ++channel) { tarmac.BaseColour[channel] = 1.0f; }
      {
        const int asphalt = World.Stack.Materials().Find("asphalt");
        tarmac.Roughness = asphalt >= 0
                               ? World.Stack.Materials().At(static_cast<size_t>(asphalt)).Roughness
                               : kUnlitTint;
      }
      const MaterialInstance paved = ground.addSurface("streets", tarmac);
      const int pavedPart = ground.addPart("streets", paved);
      const bool tookPaving =
          pavedPart >= 0 &&
          ground.setPositions(
              pavedPart,
              std::span<const float>(pavement.PositionM.data(), pavement.PositionM.size())) &&
          ground.setNormals(
              pavedPart,
              std::span<const float>(pavement.NormalM.data(), pavement.NormalM.size())) &&
          ground.setColours(
              pavedPart,
              std::span<const float>(pavement.ColourRgba.data(), pavement.ColourRgba.size())) &&
          ground.setTriangles(
              pavedPart, std::span<const uint32_t>(pavement.Index.data(), pavement.Index.size()));
      Published.Places(
          "streets: the surface they were given", static_cast<double>(paved.index()), "index");
      Published.Places(
          "streets: the part they were given", static_cast<double>(pavedPart), "index");
      Published.Places("streets: the geometry took them", tookPaving ? 1.0 : 0.0, "yes/no");
      Published.Places(
          "streets: parts the geometry now holds", static_cast<double>(ground.parts()), "parts");
    }
  }

  {
    const Ground::BuildingField &pads = World.Stack.Footprints();
    const Ground::OsmField *const shapes = World.Stack.Vectors();
    std::vector<Yields> yielding;
    if (shapes != nullptr) {
      const std::span<const double> points = shapes->Points();
      for (const Ground::BuildingField::Footprint &one : pads.Footprints()) {
        if (one.PointCount < 3) { continue; }
        Yields made;
        made.RingEastSouthM.reserve(static_cast<size_t>(one.PointCount) * 2u);
        made.LowE = kBeyondAnyCoordinate;
        made.HighE = -kBeyondAnyCoordinate;
        made.LowS = kBeyondAnyCoordinate;
        made.HighS = -kBeyondAnyCoordinate;
        bool whole = true;
        for (uint32_t step = 0; step < one.PointCount && whole; ++step) {
          const size_t at = (static_cast<size_t>(one.FirstPoint) + step) * 2u;
          if (at + 1 >= points.size()) {
            whole = false;
            break;
          }
          double eastM = 0.0;
          double upM = 0.0;
          double northM = 0.0;
          standing.Place(
              points[at], points[at + 1], static_cast<double>(one.SeatM), &eastM, &upM, &northM);
          made.RingEastSouthM.push_back(eastM);
          made.RingEastSouthM.push_back(-northM);
          made.LowE = std::min(made.LowE, eastM);
          made.HighE = std::max(made.HighE, eastM);
          made.LowS = std::min(made.LowS, -northM);
          made.HighS = std::max(made.HighS, -northM);
        }
        if (!whole) { continue; }
        {
          const size_t first = static_cast<size_t>(one.FirstPoint) * 2u;
          double eastM = 0.0;
          double upM = 0.0;
          double northM = 0.0;
          standing.Place(points[first],
                         points[first + 1],
                         static_cast<double>(one.SeatM),
                         &eastM,
                         &upM,
                         &northM);
          made.PlateauM = upM;
        }
        made.ApronM = kPadApronM;
        made.YieldM = std::fabs(static_cast<double>(one.SeatM) - static_cast<double>(one.BaseM));
        made.SeamEastSouthM = made.RingEastSouthM;
        yielding.push_back(std::move(made));
      }
    }
    const size_t builtPads = yielding.size();
    yielding.insert(yielding.end(),
                    std::make_move_iterator(corridor.begin()),
                    std::make_move_iterator(corridor.end()));
    Yielded told;
    YieldGround(std::span<const Yields>(yielding),
                kFinestGroundM,
                kMostYieldTriangles,
                GroundMesh{.PositionM = &inFrame,
                           .NormalM = &laid->NormalM,
                           .ColourRgba = tinted.empty() ? nullptr : &tinted,
                           .Uv = classUv.empty() ? nullptr : &classUv,
                           .Index = &laid->Index},
                told);
    Published.Places("ground: pads that may press it", static_cast<double>(builtPads), "pads");
    Published.Places("ground: corridor pieces that may press it",
                     static_cast<double>(yielding.size() - builtPads),
                     "pieces");
    Published.Places("ground: yields the budget took", static_cast<double>(told.Taken), "yields");
    Published.Places(
        "ground: yields the budget REFUSED", static_cast<double>(told.Refused), "yields");
    Published.Places(
        "ground: passes it refined the ring in", static_cast<double>(told.Passes), "passes");
    Published.Places("ground: vertices the refinement added",
                     static_cast<double>(told.VerticesAdded),
                     "vertices");
    Published.Places("ground: triangles the refinement added",
                     static_cast<double>(told.TrianglesAdded),
                     "triangles");
    Published.Places(
        "ground: the carriageway's footprint corners", static_cast<double>(told.Seams), "corners");
    Published.Places("ground: of those, a ground vertex shares the spot",
                     static_cast<double>(told.SeamsShared),
                     "corners");
    Published.Places(
        "ground: ring vertices a pad pressed", static_cast<double>(told.Pressed), "vertices");
    Published.Places("ground: and the deepest it pressed", told.DeepestM, "m");
    Published.Places("ground: and the highest it filled", told.RaisedM, "m");
    {
      constexpr double kUnderCellM = 16.0;
      std::unordered_map<uint64_t, std::vector<uint32_t>> facesUnder;
      const std::vector<uint32_t> &ringIndex = laid->Index;
      for (size_t at = 0; at + 2 < ringIndex.size(); at += 3) {
        double lowE = kBeyondAnyCoordinate;
        double highE = -kBeyondAnyCoordinate;
        double lowS = kBeyondAnyCoordinate;
        double highS = -kBeyondAnyCoordinate;
        for (int corner = 0; corner < 3; ++corner) {
          const size_t one = static_cast<size_t>(ringIndex[at + static_cast<size_t>(corner)]) * 3u;
          lowE = std::min(lowE, static_cast<double>(inFrame[one]));
          highE = std::max(highE, static_cast<double>(inFrame[one]));
          lowS = std::min(lowS, static_cast<double>(inFrame[one + 2u]));
          highS = std::max(highS, static_cast<double>(inFrame[one + 2u]));
        }
        const auto fromE = static_cast<int64_t>(std::floor(lowE / kUnderCellM));
        const auto toE = static_cast<int64_t>(std::floor(highE / kUnderCellM));
        const auto fromS = static_cast<int64_t>(std::floor(lowS / kUnderCellM));
        const auto toS = static_cast<int64_t>(std::floor(highS / kUnderCellM));
        if ((toE - fromE + 1) * (toS - fromS + 1) > 64) { continue; }
        for (int64_t cellE = fromE; cellE <= toE; ++cellE) {
          for (int64_t cellS = fromS; cellS <= toS; ++cellS) {
            const auto atE = static_cast<uint64_t>(cellE + 0x20000000LL);
            const auto atS = static_cast<uint64_t>(cellS + 0x20000000LL);
            facesUnder[(atE << 32U) | atS].push_back(static_cast<uint32_t>(at));
          }
        }
      }
      double deepest = 0.0;
      double summed = 0.0;
      size_t compared = 0;
      for (size_t at = 0; at + 2 < pavement.PositionM.size(); at += 3) {
        const auto eastM = static_cast<double>(pavement.PositionM[at]);
        const auto southM = static_cast<double>(pavement.PositionM[at + 2]);
        const auto atE = static_cast<uint64_t>(
            static_cast<int64_t>(std::floor(eastM / kUnderCellM)) + 0x20000000LL);
        const auto atS = static_cast<uint64_t>(
            static_cast<int64_t>(std::floor(southM / kUnderCellM)) + 0x20000000LL);
        const auto bucket = facesUnder.find((atE << 32U) | atS);
        if (bucket == facesUnder.end()) { continue; }
        double stood = -kBeyondAnyCoordinate;
        for (const uint32_t face : bucket->second) {
          const size_t a = static_cast<size_t>(ringIndex[face]) * 3u;
          const size_t b = static_cast<size_t>(ringIndex[face + 1u]) * 3u;
          const size_t c = static_cast<size_t>(ringIndex[face + 2u]) * 3u;
          const auto aE = static_cast<double>(inFrame[a]);
          const auto aS = static_cast<double>(inFrame[a + 2u]);
          const auto bE = static_cast<double>(inFrame[b]);
          const auto bS = static_cast<double>(inFrame[b + 2u]);
          const auto cE = static_cast<double>(inFrame[c]);
          const auto cS = static_cast<double>(inFrame[c + 2u]);
          const double twice = (bS - cS) * (aE - cE) + (cE - bE) * (aS - cS);
          if (std::fabs(twice) < kLeastTurnRad) { continue; }
          const double one = ((bS - cS) * (eastM - cE) + (cE - bE) * (southM - cS)) / twice;
          const double two = ((cS - aS) * (eastM - cE) + (aE - cE) * (southM - cS)) / twice;
          const double three = 1.0 - one - two;
          if (one < -kLeastRunM || two < -kLeastRunM || three < -kLeastRunM) { continue; }
          const double upM = one * static_cast<double>(inFrame[a + 1u]) +
                             two * static_cast<double>(inFrame[b + 1u]) +
                             three * static_cast<double>(inFrame[c + 1u]);
          stood = std::max(stood, upM);
        }
        if (stood < kUnraisedDeckM) { continue; }
        const double under = stood - static_cast<double>(pavement.PositionM[at + 1]);
        ++compared;
        summed += under > 0.0 ? under : 0.0;
        deepest = under > deepest ? under : deepest;
      }
      Published.Places("streets: the deepest the ground stands over one", deepest, "m");
      Published.Places("streets: how far on average",
                       compared > 0 ? summed / static_cast<double>(compared) : 0.0,
                       "m");
      Published.Places("streets: vertices compared", static_cast<double>(compared), "vertices");
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
  if (!classUv.empty()) {
    (void)ground.setTexture(ringPart, std::span<const float>(classUv.data(), classUv.size()), 0);
  }

  {
    const Ground::WaterField &wet = World.Stack.WaterBodies();
    const Ground::OsmField *const vectors = World.Stack.Vectors();
    std::vector<float> places;
    std::vector<float> facing;
    std::vector<uint32_t> order;
    size_t lidsLaid = 0;
    size_t lidsRefused = 0;
    if (vectors != nullptr) {
      const std::span<const double> points = vectors->Points();
      for (const Ground::WaterField::Surface &lake : wet.Surfaces()) {
        if (lake.PointCount < 3) {
          ++lidsRefused;
          continue;
        }
        const size_t last = (static_cast<size_t>(lake.FirstPoint) + lake.PointCount) * 2;
        if (last > points.size()) {
          ++lidsRefused;
          continue;
        }
        const size_t began = places.size();
        const bool whole = true;
        for (uint32_t step = 1; step + 1 < lake.PointCount && whole; ++step) {
          const std::array<uint32_t, 3> corners = {{0u, step, step + 1u}};
          for (const uint32_t corner : corners) {
            const size_t at = (static_cast<size_t>(lake.FirstPoint) + corner) * 2;
            double eastM = 0.0;
            double upM = 0.0;
            double northM = 0.0;
            standing.Place(points[at],
                           points[at + 1],
                           static_cast<double>(lake.LevelM),
                           &eastM,
                           &upM,
                           &northM);
            places.push_back(static_cast<float>(eastM));
            places.push_back(static_cast<float>(upM));
            places.push_back(static_cast<float>(-northM));
            facing.push_back(0.0f);
            facing.push_back(1.0f);
            facing.push_back(0.0f);
            order.push_back(static_cast<uint32_t>(order.size()));
          }
        }
        if (places.size() > began) {
          ++lidsLaid;
        } else {
          ++lidsRefused;
        }
      }
    }
    Published.Places("water: surfaces laid", static_cast<double>(lidsLaid), "surfaces");
    Published.Places("water: surfaces refused", static_cast<double>(lidsRefused), "surfaces");
    Published.Places("water: triangles", static_cast<double>(order.size() / 3), "triangles");
    if (order.size() >= 3) {
      Material lagoon;
      lagoon.BaseColour[0] = kLagoonRed;
      lagoon.BaseColour[1] = kLagoonGreen;
      lagoon.BaseColour[2] = kLagoonBlue;
      lagoon.Roughness = kLagoonRoughness;
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
  const Render::Medium air = Render::kEarthAir;
  Material wearing;
  for (int channel = 0; channel < 3; ++channel) {
    wearing.BaseColour[channel] = air.GroundAlbedo[channel];
  }

  const size_t drivenParts = Picture.Standing->CarriedParts();
  Published.Places("restand: the carried count the world hands over",
                   static_cast<double>(drivenParts),
                   "carried");
  Published.Places("restand: parts in the geometry", static_cast<double>(ground.parts()), "parts");
  Published.Places(
      "rebuild: and assembling one subject took",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt).count(),
      "ms");
  phaseAt = std::chrono::steady_clock::now();
  Picture.Standing->GroundIs(ringSurface.index());
  if (classStructure && !classPalette.empty() &&
      !Picture.Standing->GroundClasses(classStructure->Words(),
                                       classStructure->Bytes() / sizeof(uint32_t),
                                       classPalette.data(),
                                       classPalette.size(),
                                       Error)) {
    return false;
  }
  Picture.Standing->Digests(declared.Render.Audits);
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
    static const std::array<const char *const, 3> kSky = {"the ambient the sky casts, red",
                                                          "the ambient the sky casts, green",
                                                          "the ambient the sky casts, blue"};
    static const std::array<const char *const, 3> kGround = {
        "the ambient the ground bounces, red",
        "the ambient the ground bounces, green",
        "the ambient the ground bounces, blue"};
    for (size_t at = 0; at < 3; ++at) {
      Published.Places(kSky[at], Picture.Standing->AmbientStood()[at], "");
      Published.Places(kGround[at], Picture.Standing->GroundStood()[at], "");
    }
  }
  Published.Places("stand: times the sky was integrated",
                   static_cast<double>(Picture.Standing->SkyIntegrations()),
                   "integrations");
  Published.Places("stand: sweeping the bounds to frame it", Picture.Standing->FramingMs(), "ms");
  Published.Places("rebuild: resolving its surface", Picture.Standing->ResolveMs(), "ms");
  Published.Places("rebuild: and its bounds", Picture.Standing->BoundsMs(), "ms");
  Published.Places("rebuild: cutting it into clusters", Render::CookedMs(), "ms");
  Published.Places("cook: clusters with no parent above them",
                   static_cast<double>(Render::CookedRootless()),
                   "clusters");
  Published.Places(
      "cook: clusters in all", static_cast<double>(Render::CookedClusters()), "clusters");
  Published.Places("rebuild: of the streams, packing them", Render::PackedMs(), "ms");
  Published.Places(
      "restand: the geometry handed over, digested", Render::HandedGeometryDigest(), "");
  Published.Places("rebuild: digesting what it handed over", Render::DigestedMs(), "ms");
  Published.Places("rebuild: and the device taking them", Render::HandedMs(), "ms");
  Published.Places("rebuild: uploads the residency made",
                   static_cast<double>(Render::SubjectResidency::UploadsTaken()),
                   "uploads");
  Published.Places("rebuild: megabytes they carried",
                   static_cast<double>(Render::SubjectResidency::UploadMBTaken()),
                   "MB");
  Published.Places("rebuild: device buffers created",
                   static_cast<double>(Render::SubjectResidency::BuffersMadeTaken()),
                   "buffers");
  Published.Places("rebuild: staging buffers created",
                   static_cast<double>(Render::SubjectResidency::StagingMadeTaken()),
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
                   static_cast<double>(Picture.Standing->PartsStanding()),
                   "parts");
  Published.Places("restand: instances it carries",
                   static_cast<double>(Picture.Standing->InstancesStanding()),
                   "instances");
  Published.Places(
      "restand: the near plane the renderer stands on", Picture.Standing->NearStanding(), "m");
  for (size_t part = 0; part < Picture.Standing->Shown().Parts.size(); ++part) {
    const Render::ShapePart &one = Picture.Standing->Shown().Parts[part];
    Published.Places("restand: subject part " + std::to_string(part) + " first vertex",
                     static_cast<double>(one.FirstVertex),
                     "");
    Published.Places("restand: subject part " + std::to_string(part) + " vertex count",
                     static_cast<double>(one.VertexCount),
                     "");
    Published.Places("restand: subject part " + std::to_string(part) + " first index",
                     static_cast<double>(one.FirstIndex),
                     "");
    Published.Places("restand: subject part " + std::to_string(part) + " index count",
                     static_cast<double>(one.IndexCount),
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
  Published.Places("tiles the ring laid", static_cast<double>(laid->Tiles), "tiles");
  Published.Places("tiles it is still waiting for", static_cast<double>(laid->Pending), "tiles");
  Published.Places("tiles the stack does not hold", static_cast<double>(laid->Absent), "tiles");
  Published.Places("tiles it refused", static_cast<double>(laid->Refused), "tiles");
  {
    double least = kBeyondAnyCoordinate;
    double most = -kBeyondAnyCoordinate;
    for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
      const auto up = static_cast<double>(inFrame[at + 1]);
      least = std::min(up, least);
      most = std::max(up, most);
    }
    double west = kBeyondAnyCoordinate;
    double east = -kBeyondAnyCoordinate;
    double north = kBeyondAnyCoordinate;
    double south = -kBeyondAnyCoordinate;
    for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
      const auto alongE = static_cast<double>(inFrame[at]);
      const auto alongS = static_cast<double>(inFrame[at + 2]);
      west = std::min(alongE, west);
      east = std::max(alongE, east);
      north = std::min(alongS, north);
      south = std::max(alongS, south);
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
        summed += static_cast<double>(inFrame[at + 1]);
        ++counted;
      }
      const double mean = counted > 0 ? summed / static_cast<double>(counted) : 0.0;
      size_t adrift = 0;
      for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
        const double off = static_cast<double>(inFrame[at + 1]) - mean;
        if (off > kAdriftMostM || off < -kAdriftMostM) { ++adrift; }
      }
      Published.Places("the height its vertices average", mean, "m");
      Published.Places(
          "vertices more than 500 m from that average", static_cast<double>(adrift), "vertices");
      Published.Places("out of", static_cast<double>(counted), "vertices");
    }
  }
  Published.Places("the sun stands this high", Picture.Standing->Standing().KeyElevationDeg, "deg");
  Published.Places("and bears", Picture.Standing->Standing().KeyBearingDeg, "deg");
  Published.Places("the light that reaches the ground", Picture.Standing->MeteredLux(), "lux");
  Published.Places("and the exposure metered from it",
                   Picture.Standing->Standing().KeyFromClock ? 1.0 : 0.0,
                   "yes/no");
  Published.Places("times the terrain was rebuilt", static_cast<double>(World.Relaid), "rebuilds");
  ++World.Rebuilds;
  World.RebuildMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - rebuildBegan)
          .count();
  Published.Places("and what the last rebuild took", World.RebuildMs, "ms");
  Published.Places(
      "rebuild: times the world was built WHOLE", static_cast<double>(World.Rebuilds), "rebuilds");
  Published.Places("and how often it was asked about", static_cast<double>(World.Asked), "walks");
  Published.Places(
      "levels the cascade laid", static_cast<double>(over.Zoom - laid->CoarsestZoom + 1), "levels");
  Published.Places(
      "tiles it skipped as already covered", static_cast<double>(laid->Skipped), "tiles");
  Published.Places("tiles the last rebuild laid bare", static_cast<double>(laid->Bare), "tiles");
  World.Pending = laid->Pending;
  World.Bare = laid->Bare;
  World.Wanted = laid->Tiles;
  Published.Places(
      "tiles that overlap a finer level", static_cast<double>(laid->Overlapped), "tiles");
  Published.Places("clusters the ring holds", static_cast<double>(laid->ClustersHeld), "clusters");
  Published.Places("clusters it drew", static_cast<double>(laid->ClustersDrawn), "clusters");
  Published.Places("ring: clusters carried for the device",
                   static_cast<double>(laid->Clusters.size()),
                   "clusters");
  Published.Places("ring: the whole index list they cut from",
                   static_cast<double>(laid->AllIndex.size()),
                   "indices");
  Published.Places("ring: against the list the CPU selected",
                   static_cast<double>(laid->Index.size()),
                   "indices");
  Published.Places("ring: clusters it holds", static_cast<double>(laid->ClustersHeld), "clusters");
  Published.Places(
      "ring: clusters the CPU drew", static_cast<double>(laid->ClustersDrawn), "clusters");
  Published.Places("the worst error any of them carries", laid->WorstErrM, "m");
  return true;
}

bool Engine::State::Stood() {
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
  for (size_t at = 0; at < positionsM.size(); ++at) {
    corners[at] = static_cast<float>(positionsM[at]);
  }
  World.Blocking =
      TriangleBvh::Over(Span<const float>(corners.data(), corners.size()),
                        Span<const uint32_t>(standing.Indices().data(), standing.Indices().size()));
}

void Engine::State::Tells() {
  const Heap::Tagged telling("frame-tells");
  for (size_t at = 0; at < Heap::TagCount(); ++at) {
    const char *const tag = Heap::TagAt(at);
    if (tag == nullptr || Heap::TakenAt(at) == 0) { continue; }
    Published.Places(
        std::string("heap taken under ") + tag, static_cast<double>(Heap::TakenAt(at)), "bytes");
  }
  if (Cost.Advance.Count > 0) {
    Published.Places("the step's own time, last", Cost.Advance.LastMs, "ms");
    Published.Places("the step's own time, least", Cost.Advance.LeastMs, "ms");
    Published.Places("the step's own time, most", Cost.Advance.MostMs, "ms");
    Published.Places("steps taken", static_cast<double>(Cost.Advance.Count), "steps");
  }
  if (Picture.Standing) {
    for (size_t at = 0; at < Render::kStageCount; ++at) {
      const auto stage = static_cast<Render::Stage>(at);
      const Render::SceneRenderer::Effort &spent = Picture.Device.Spent(stage);
      if (spent.TookMs <= 0.0 && spent.Draws == 0) { continue; }
      Published.Places(std::string(Row(stage).Name) + ", took", spent.TookMs, "ms");
      Published.Places(
          std::string(Row(stage).Name) + ", drew", static_cast<double>(spent.Draws), "draws");
      Published.Places(std::string(Row(stage).Name) + ", triangles",
                       static_cast<double>(spent.Triangles),
                       "triangles");
      Published.Places(std::string(Row(stage).Name) + ", surfaces",
                       static_cast<double>(spent.Surfaces),
                       "slots");
      Published.Places(std::string(Row(stage).Name) + ", placements",
                       static_cast<double>(spent.Placements),
                       "slots");
      Published.Places(std::string(Row(stage).Name) + ", textured",
                       static_cast<double>(spent.Textured),
                       "slots");
      Published.Places(std::string(Row(stage).Name) + ", colour images",
                       static_cast<double>(spent.Palettes),
                       "images");
      Published.Places(std::string(Row(stage).Name) + ", device bytes",
                       static_cast<double>(spent.DeviceBytes),
                       "bytes");
      Published.Places(std::string(Row(stage).Name) + ", placements that differ",
                       static_cast<double>(spent.Distinct),
                       "rows");
      Published.Places(std::string(Row(stage).Name) + ", vertex layouts",
                       static_cast<double>(spent.Layouts),
                       "layouts");
    }
  }
  if (Cost.Render.Count > 0) {
    Published.Places("the picture's own time, last", Cost.Render.LastMs, "ms");
    Published.Places("the picture's own time, least", Cost.Render.LeastMs, "ms");
    Published.Places("the picture's own time, most", Cost.Render.MostMs, "ms");
    Published.Places("pictures drawn", static_cast<double>(Cost.Render.Count), "pictures");
  }
  {
    const std::vector<std::string> clashed = Published.Clashed();
    Published.Places(
        "measures published twice in one round", static_cast<double>(clashed.size()), "rows");
    for (const std::string &one : clashed) {
      Published.Places("published twice in one round: " + one, 1.0, "rows");
    }
  }

  const unsigned next = (Session.Told.load(std::memory_order_relaxed) + 1u) & 1u;
  std::vector<Audio::Heard> &sources = Session.Sources[next];
  sources.clear();
  sources.reserve(Session.Declared.Sounds.size());
  for (const Scenario::Sound &declared : Session.Declared.Sounds) {
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

bool Engine::State::Blocked(const Vec3 &sourceM) const {
  if (World.Blocking.Empty() || !Picture.Standing) { return false; }
  const Render::Viewpoint &eye = Picture.Standing->Aimed();
  Vec3f fromM;
  Vec3f along;
  double awayM = 0.0;
  for (int axis = 0; axis < 3; ++axis) {
    const double step = sourceM[axis] - eye.EyeM[axis];
    awayM += step * step;
  }
  awayM = std::sqrt(awayM);
  if (!(awayM > 0.0)) { return false; }
  for (int axis = 0; axis < 3; ++axis) {
    fromM[axis] = static_cast<float>(eye.EyeM[axis]);
    along[axis] = static_cast<float>((sourceM[axis] - eye.EyeM[axis]) / awayM);
  }
  return World.Blocking.Occludes(fromM, along, kNearestOccluderM, static_cast<float>(awayM));
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
  return S_->Session.Sounding.Fills(
             stereo, S_->Session.Sources[told], S_->Session.Ear[told], S_->Error)
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
  S_->Published.Places(
      "subject draws", static_cast<double>(S_->Picture.Device.SubjectDrawCount()), "draws");
  S_->Published.Places(
      "the subject's own animation runs for", S_->Picture.Standing->DurationS(), "s");
  S_->Published.Places("the frames its rate makes of that",
                       static_cast<double>(S_->Picture.Standing->Frames()),
                       "frames");
  S_->Published.Places("and the instant it is posed at", S_->Picture.Standing->AtS(), "s");
  S_->Published.Places(
      "the pose's own local transforms, digested", S_->Picture.Standing->LocalsDigest(), "");
  S_->Published.Places(
      "the vertices it assembled from them, digested", S_->Picture.Standing->AssembledDigest(), "");
  S_->Published.Places(
      "the geometry the renderer was last offered, digested", Render::HandedGeometryDigest(), "");
  S_->Published.Places("uploads the subject residency has made in all",
                       static_cast<double>(Render::SubjectResidency::UploadsEver()),
                       "uploads");
  S_->Published.Places("staged crossings the residency flushed",
                       static_cast<double>(Render::SubjectResidency::CrossingsFlushed()),
                       "crossings");
  S_->Published.Places("subject clusters",
                       static_cast<double>(S_->Picture.Standing->Shown().Clusters.size()),
                       "clusters");
  S_->Published.Places("cull: jobs it swept",
                       static_cast<double>(Render::SubjectCullStage::JobsSweptTaken()),
                       "jobs");
  if (S_->Session.Declared.Render.Audits) {
    float nearest = 0.0f;
    float farthest = 0.0f;
    float mean = 0.0f;
    if (S_->Picture.Standing->Pyramid(nearest, farthest, mean) == Render::ReadState::Ready) {
      S_->Published.Places(
          "cull: the pyramid's nearest depth", static_cast<double>(nearest), "0..1");
      S_->Published.Places("cull: its farthest", static_cast<double>(farthest), "0..1");
      S_->Published.Places("cull: and its mean", static_cast<double>(mean), "0..1");
    }
  }
  {
    const Render::Viewpoint &eye = S_->Picture.Standing->Aimed();
    const double aspect = S_->Picture.Frame.HeightPx > 0
                              ? static_cast<double>(S_->Picture.Frame.WidthPx) /
                                    static_cast<double>(S_->Picture.Frame.HeightPx)
                              : 1.0;
    const double half = 0.5 * eye.YfovRad;
    const double up = std::tan(half);
    const double across = up * aspect;
    size_t kept = 0;
    for (const DagCluster &one : S_->Picture.Standing->Shown().Clusters) {
      const Vec3 to = {{static_cast<double>(one.SelfCenter[0]) - eye.EyeM[0],
                        static_cast<double>(one.SelfCenter[1]) - eye.EyeM[1],
                        static_cast<double>(one.SelfCenter[2]) - eye.EyeM[2]}};
      const double ahead = to[0] * eye.Forward[0] + to[1] * eye.Forward[1] + to[2] * eye.Forward[2];
      const double right = to[0] * eye.Right[0] + to[1] * eye.Right[1] + to[2] * eye.Right[2];
      const double over = to[0] * eye.Up[0] + to[1] * eye.Up[1] + to[2] * eye.Up[2];
      const auto radius = static_cast<double>(one.SelfRadius);
      if (ahead + radius < eye.ZNearM) { continue; }
      if (eye.ZFarM > 0.0 && ahead - radius > eye.ZFarM) { continue; }
      if (std::fabs(right) - radius > across * (ahead > 0.0 ? ahead : 0.0) + radius) { continue; }
      if (std::fabs(over) - radius > up * (ahead > 0.0 ? ahead : 0.0) + radius) { continue; }
      ++kept;
    }
    S_->Published.Places(
        "ring: clusters a frustum would keep", static_cast<double>(kept), "clusters");
  }
  S_->Published.Places(
      "subject draw calls", static_cast<double>(S_->Picture.Device.SubjectBatchCount()), "calls");
  S_->Published.Places(
      "plan passes", static_cast<double>(S_->Picture.Standing->PlanPasses()), "passes");
  for (uint32_t at = 0; at < static_cast<uint32_t>(Render::kVertexLayouts.size()); ++at) {
    const uint32_t many =
        S_->Picture.Device.SubjectBatchesTaking(static_cast<Render::VertexLayout>(at));
    if (many == 0) { continue; }
    S_->Published.Places(
        "draws taking vertex layout " + std::to_string(at), static_cast<double>(many), "draws");
  }
  S_->Drew();
  return true;
}

Result Engine::inspect() {
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

Extent Engine::canvas() const {
  return S_->Picture.Frame;
}

bool Engine::camera(Scenario::Camera &out) const {
  if (!S_->Picture.Standing) { return false; }
  Render::CameraOf(S_->Picture.Standing->Aimed(), out);
  return true;
}

bool Engine::presenting() const {
  return S_->Picture.Device.Presents();
}

bool Engine::beginFrame() {
  if (!S_->Stood()) { return false; }
  if (!S_->Picture.Standing) {
    S_->Error = "a frame is begun over a scenario, and none stands";
    return false;
  }
  S_->Picture.FrameOpen = true;
  return true;
}

bool Engine::endFrame() {
  if (!S_->Picture.FrameOpen) {
    S_->Error = "a frame was ended that was never begun";
    return false;
  }
  S_->Picture.FrameOpen = false;
  if (!S_->Picture.Standing) { return true; }
  return S_->Picture.Standing->Present(S_->Error);
}

bool Engine::flushAndWait() {
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
