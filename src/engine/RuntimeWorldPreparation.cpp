#include "ShippedProviders.h"
#include "EngineHeld.h"
#include "Heap.h"
#include "OfflineTransport.h"
#include "math/Units.h"
#include "OsmXmlReader.h"
#include "OsmSourceDemand.h"

#include <cmath>
#include <cstddef>
#include <algorithm>
#include <array>
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
  Generators::RegisterShippedProviders(World.Providers);
  std::vector<Data::SourceProvider> osmProviders;
  tileProviders.reserve(Session.Declared.Providers.size());
  osmProviders.reserve(Session.Declared.Providers.size());
  for (const Data::SourceProvider &provider : Session.Declared.Providers) {
    (provider.Kind == "osm" ? osmProviders : tileProviders).push_back(provider);
  }
  std::vector<Generators::Osm::CircuitRequest> routes;
  routes.reserve(Session.Declared.Routes.size());
  for (const Scenario::RouteDeclaration &declared : Session.Declared.Routes) {
    routes.push_back({.Id = declared.Id, .RelationId = declared.OsmRelationId});
  }
  if (!routes.empty() && osmProviders.empty()) {
    Error = "semantic OSM routes require a source";
    return false;
  }
  if (osmProviders.empty() && !World.OsmSource && !World.OsmTransport) { return true; }
  if (!World.Pool) { World.Pool = std::make_unique<Tasks>(Tasks::ComputeThreads()); }
  if (!World.Io) { World.Io = std::make_unique<Tasks>(1); }
  if (!World.OsmSource) {
    if (!World.Wire) {
      if (Session.Under.Offline) {
        World.Wire = std::make_unique<Data::OfflineTransport>();
      } else {
        World.Wire =
            std::make_unique<Fetching>(Fetching::Config{.ConcurrentTransfers = 8,
                                                        .ConnectionsPerHost = 8,
                                                        .MaxBodyBytes = Data::kMaxOsmXmlBytes});
      }
    }
    World.OsmSource = std::make_unique<Generators::Osm::SourceAcquisition>(
        Generators::Osm::SourceAcquisition::Workers{.Compute = *World.Pool, .Io = *World.Io},
        World.Wire.get(),
        Session.Under.Cache);
  }
  World.OsmRoutes = std::move(routes);
  World.OriginalSourceDemand.reset();
  if (!RequestOsmSources(osmProviders)) { return false; }
  if (World.OsmRoutes.empty()) {
    World.OsmTransport.reset();
  } else if (!World.OsmTransport) {
    World.OsmTransport = std::make_unique<Generators::Osm::TransportPreparation>(*World.Pool);
  }
  if (World.OsmSource->CurrentPhase() == Generators::Osm::SourceAcquisition::Phase::Ready ||
      World.OsmSource->CurrentPhase() == Generators::Osm::SourceAcquisition::Phase::Inactive) {
    return SubmitOsmTransportSource();
  }
  return true;
}

bool Engine::State::RequestOsmSources(std::span<const Data::SourceProvider> providers) {
  if (providers.size() == 1 && !providers.front().Coverage && providers.front().Location.empty()) {
    if (!World.OsmRoutes.empty()) {
      Error = "OSM catalogue routes require native cell transport integration";
      return false;
    }
    return RequestOriginalCells();
  }
  if (auto requested = World.OsmSource->Request(providers, Session.Under.Shipped, &World.Providers);
      !requested) {
    Error = std::move(requested.error());
    return false;
  }
  return true;
}

bool Engine::State::RequestOriginalCells() {
  if (!World.OsmSource) { return true; }
  const auto found = std::ranges::find_if(Session.Declared.Providers, [](const auto &provider) {
    return provider.Kind == "osm" && !provider.Coverage && provider.Location.empty();
  });
  if (found == Session.Declared.Providers.end()) { return true; }
  const auto focus = CurrentGeographicFocus();
  const std::array demand{focus.LatitudeDeg, focus.LongitudeDeg, TerrainSightM()};
  if (World.OriginalSourceDemand == demand) { return true; }
  auto cells = Data::CellsAround(demand[0],
                                 demand[1],
                                 demand[2],
                                 Generators::Osm::kCatalogueCellLevel,
                                 Generators::Osm::kDefaultCellLimits.CellsMost);
  if (!cells) {
    Error = std::move(cells.error());
    return false;
  }
  if (auto requested = World.OsmSource->RequestCells(*found,
                                                     *cells,
                                                     Generators::Osm::kDefaultCellLimits,
                                                     Session.Under.Shipped,
                                                     &World.Providers);
      !requested) {
    Error = std::move(requested.error());
    return false;
  }
  World.OriginalSourceDemand = demand;
  Published.RecordMetric(
      "original OSM requested cells", static_cast<double>(cells->size()), "cells");
  return true;
}

bool Engine::State::SubmitOsmTransportSource() {
  if (!World.OsmTransport) { return true; }
  auto requested = World.OsmTransport->RequestSource(World.OsmSource->Current(), World.OsmRoutes);
  if (!requested) {
    Error = std::move(requested.error());
    return false;
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
  Published.RecordMetric(
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
    const auto shipped = Generators::ShippedProviders();
    tileProviders.assign(shipped.begin(), shipped.end());
  }
  Collecting say;
  const World::StoragePaths worldStorage{.Shipped = Session.Under.Shipped,
                                         .Cache = Session.Under.Cache};
  if (!World.Pool) { World.Pool = std::make_unique<Tasks>(Tasks::ComputeThreads()); }
  if (!World.Stack.Opened() && !World.Stack.Open(worldStorage,
                                                 tileProviders,
                                                 {.LongitudeDeg = atLon, .LatitudeDeg = atLat},
                                                 *World.Wire,
                                                 *World.Pool,
                                                 say,
                                                 Diagnostics,
                                                 Session.Declared.Ground.PatienceS,
                                                 &World.Providers)) {
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
    Published.RecordMetric("ground: a declared relief stands in for the tiles", 1.0, "yes/no");
    const double hereM =
        World.Stack.Ground().At({.LongitudeDeg = atLon, .LatitudeDeg = atLat}).AslM().value_or(0.0);
    const double eastM = World.Stack.Ground()
                             .At({.LongitudeDeg = atLon + kEastStepDeg, .LatitudeDeg = atLat})
                             .AslM()
                             .value_or(0.0);
    Published.RecordMetric("ground: the relief says this at the origin", hereM, "m");
    Published.RecordMetric("ground: and this a kilometre east", eastM, "m");
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
    if (!World.Shipping.EnsureCatalogue(World.Stack.Vegetation(),
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

void Engine::State::PollOsmSources() {
  if (World.OsmSource) {
    const auto previousRevision = World.OsmSource->PublishedRevision();
    World.OsmSource->Poll();
    if (World.OsmSource->PublishedRevision() != previousRevision && World.CurrentOriginalReady() &&
        World.OsmSource->Current()) {
      const auto &source = *World.OsmSource->Current();
      Published.RecordMetric(
          "original OSM source bytes", static_cast<double>(source.SourceBytes), "bytes");
      Published.RecordMetric("original OSM read time", source.ReadMs, "ms");
      Published.RecordMetric("original OSM parse time", source.ParseMs, "ms");
      if (!World.OsmTransport) {
        Published.RecordMetric(
            "semantic OSM source bytes", static_cast<double>(source.SourceBytes), "bytes");
        Published.RecordMetric("semantic OSM read time", source.ReadMs, "ms");
        Published.RecordMetric("semantic OSM parse time", source.ParseMs, "ms");
      }
      if (!SubmitOsmTransportSource()) { return; }
    }
  }
  const auto previous = World.OsmTransport ? World.OsmTransport->Current() : nullptr;
  if (World.OsmTransport) { World.OsmTransport->Poll(); }
  Published.RecordMetric(
      "semantic OSM jobs pending",
      static_cast<double>((World.OsmTransport ? World.OsmTransport->PendingCount() : 0) +
                          (World.OsmSource ? World.OsmSource->PendingCount() : 0)),
      "jobs");
  if (!World.OsmTransport) { return; }
  Published.RecordMetric("semantic OSM jobs completed",
                         static_cast<double>(World.OsmTransport->CompletedCount()),
                         "jobs");
  Published.RecordMetric("semantic OSM jobs canceled",
                         static_cast<double>(World.OsmTransport->CanceledCount()),
                         "jobs");
  const auto &current = World.OsmTransport->Current();
  if (!current || current == previous) { return; }
  const World::TransportLoadMetrics &metrics = current->Metrics();
  Published.RecordMetric(
      "semantic OSM source bytes", static_cast<double>(metrics.SourceBytes), "bytes");
  Published.RecordMetric("semantic OSM read time", metrics.ReadMs, "ms");
  Published.RecordMetric("semantic OSM parse time", metrics.ParseMs, "ms");
  Published.RecordMetric("semantic OSM graph time", metrics.GraphMs, "ms");
  Published.RecordMetric(
      "semantic OSM named routes", static_cast<double>(current->RouteCount()), "routes");
  Published.RecordMetric(
      "semantic OSM route edges", static_cast<double>(current->RouteEdgeCount()), "edges");
}

}
