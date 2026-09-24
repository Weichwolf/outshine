#include "EngineHeld.h"
#include "Heap.h"
#include "OfflineTransport.h"
#include "math/Units.h"

#include <cmath>
#include <memory>
#include <numbers>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine {

namespace {

struct OsmLayerTraits {
  bool Area = false;
  bool Wet = false;
};

Ground::OsmLayer SelectOsmLayer(OsmLayerTraits traits) {
  if (traits.Area) {
    return traits.Wet ? Ground::OsmLayer::WaterPolygons : Ground::OsmLayer::Buildings;
  }
  return traits.Wet ? Ground::OsmLayer::WaterLines : Ground::OsmLayer::Streets;
}

constexpr double kEastStepDeg = 0.0138;

}

bool Engine::State::ConfigureSourceProviders(std::vector<Data::SourceProvider> &tileProviders) {
  std::vector<Data::SourceProvider> osmProviders;
  tileProviders.reserve(Session.Declared.Providers.size());
  osmProviders.reserve(Session.Declared.Providers.size());
  for (const Data::SourceProvider &provider : Session.Declared.Providers) {
    (provider.Kind == "osm" ? osmProviders : tileProviders).push_back(provider);
  }
  std::vector<World::OsmCircuitRequest> routes;
  routes.reserve(Session.Declared.Routes.size());
  for (const Scenario::RouteDeclaration &declared : Session.Declared.Routes) {
    routes.push_back({.Id = declared.Id, .RelationId = declared.OsmRelationId});
  }
  if (!osmProviders.empty() || World.OsmTransportLoader) {
    if (!World.Pool) { World.Pool = std::make_unique<Tasks>(Tasks::ComputeThreads()); }
    if (!World.OsmTransportLoader) {
      World.OsmTransportLoader = std::make_unique<World::OsmTransportLoader>(*World.Pool);
    }
    if (auto requested =
            World.OsmTransportLoader->Request(osmProviders, Session.Under.Shipped, routes);
        !requested) {
      Error = std::move(requested.error());
      return false;
    }
  }
  return true;
}

void Engine::State::DeclareGroundFeatures() {
  if (Session.Declared.Ground.Osm.empty()) { return; }
  std::vector<Ground::OsmField::Declared> told;
  told.reserve(Session.Declared.Ground.Osm.size());
  for (const Scenario::Structure &one : Session.Declared.Ground.Osm) {
    Ground::OsmField::Declared made;
    const bool wet = one.Kind == "water";
    const Ground::OsmLayer holds = SelectOsmLayer({.Area = one.Area, .Wet = wet});
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

bool Engine::State::PrepareRuntimeWorld() {
  static const Heap::Tag kComposingTag("world-compose");
  const Heap::Tagged composing(kComposingTag);
  World.GroundTiles = 0;
  if (!EnsureRuntimeScene()) { return false; }
  const Scenario::Document &declared = Session.Declared;
  if (Session.Views &&
      (Session.Views->Active().Placement != Scenario::CameraPlacement::FollowEntity) &&
      !Session.Views->Active().Geographic.SamplesHeight && !UpdateActiveCamera()) {
    return false;
  }
  std::vector<Data::SourceProvider> tileProviders;
  if (!ConfigureSourceProviders(tileProviders)) { return false; }
  if (!declared.Ground.Declared) { return true; }
  const double atLat = declared.Ground.Origin.LatitudeDeg;
  const double atLon = declared.Ground.Origin.LongitudeDeg;
  if (!World.Wire) {
    if (Session.Under.Offline) {
      World.Wire = std::make_unique<Data::OfflineTransport>();
    } else {
      World.Wire = std::make_unique<Fetching>(Fetching::Config{});
    }
  }

  if (tileProviders.empty()) {
    const auto shipped = Data::ShippedProviders();
    tileProviders.assign(shipped.begin(), shipped.end());
  }
  Collecting say;
  const World::StoragePaths worldStorage{.Shipped = Session.Under.Shipped,
                                         .Cache = Session.Under.Cache};
  if (!World.Stack.Opened() && !World.Stack.Open(worldStorage,
                                                 tileProviders,
                                                 {.LongitudeDeg = atLon, .LatitudeDeg = atLat},
                                                 *World.Wire,
                                                 say,
                                                 Diagnostics,
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

  DeclareGroundFeatures();

  {
    const double fovDeg =
        Session.Declared.Views.empty() || Session.Declared.Views.front().Sees.FovDeg <= 0.0
            ? Scenario::kFovUnsaidDeg
            : Session.Declared.Views.front().Sees.FovDeg;
    const double highPx = Session.Declared.Render.Frame.HeightPx > 0
                              ? static_cast<double>(Session.Declared.Render.Frame.HeightPx)
                              : static_cast<double>(kFrameUnsaidHighPx);
    World.Stack.SeeFootprintsWith(highPx /
                                  (2.0 * std::tan(fovDeg * std::numbers::pi / kDegPerTurn)));
    const int vectorZoom = World.Stack.VectorZoom();
    const double vectorSpanM = Data::kMercatorGirthM *
                               std::cos(Session.Declared.Ground.Origin.LatitudeDeg * kDeg2Rad) /
                               std::ldexp(1.0, vectorZoom);
    World.Stack.FootprintTilesSpan(vectorSpanM);
  }
  if (World.Stack.Vegetated()) {
    std::string why;
    if (!World.Shipping.Stands(World.Stack.Vegetation(),
                               std::string(Session.Under.Shipped) + "/world/species",
                               why,
                               declared.Ground.VegetationEnabled)) {
      Error = std::move(why);
      return false;
    }
    World.Table = Generators::TableOf(World.Stack.Vegetation());
  }

  HandsPiecesOver();
  return Grounds(true, GroundQuality::Playable);
}

void Engine::State::PollOsmTransport() {
  if (!World.OsmTransportLoader) { return; }
  const auto previous = World.OsmTransportLoader->Current();
  World.OsmTransportLoader->Poll();
  Published.Places("semantic OSM jobs pending",
                   static_cast<double>(World.OsmTransportLoader->PendingCount()),
                   "jobs");
  Published.Places("semantic OSM jobs completed",
                   static_cast<double>(World.OsmTransportLoader->CompletedCount()),
                   "jobs");
  Published.Places("semantic OSM jobs canceled",
                   static_cast<double>(World.OsmTransportLoader->CanceledCount()),
                   "jobs");
  const auto &current = World.OsmTransportLoader->Current();
  if (!current || current == previous) { return; }
  const World::TransportLoadMetrics &metrics = current->Metrics();
  Published.Places("semantic OSM source bytes", static_cast<double>(metrics.SourceBytes), "bytes");
  Published.Places("semantic OSM read time", metrics.ReadMs, "ms");
  Published.Places("semantic OSM parse time", metrics.ParseMs, "ms");
  Published.Places("semantic OSM graph time", metrics.GraphMs, "ms");
  Published.Places(
      "semantic OSM named routes", static_cast<double>(current->RouteCount()), "routes");
  Published.Places(
      "semantic OSM route edges", static_cast<double>(current->RouteEdgeCount()), "edges");
}

}
