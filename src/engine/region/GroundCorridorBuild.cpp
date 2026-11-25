#include "EngineState.h"
#include "PageIndexAsset.h"
#include "WorldRegionBuildState.h"
#include "GroundWorldCandidate.h"
#include "DiagnosticLedger.h"
#include "SourceProductSelection.h"
#include "WaterSurfaceBuilder.h"
#include <chrono>
#include <cmath>
#include <cstddef>
#include <expected>
#include <format>
#include <memory>
#include <optional>
#include <ratio>
#include <string>
#include <utility>
#include <vector>

namespace outshine {
namespace {
constexpr size_t kCorridorLanesPerFrame = 128;
constexpr size_t kCorridorNodesPerFrame = 64;

std::optional<RoadPageRestoration::DisplayView> RoadDisplayView(const ViewPath &path,
                                                                const TangentFrame &frame,
                                                                const Render::Viewpoint &camera,
                                                                Vec2 viewport) {
  if (camera.Kind == Render::CameraKind::Orthographic) { return std::nullopt; }
  const auto world = ViewPathInEcef(path, frame);
  const double focal = viewport[1] / (2.0 * std::tan(camera.YfovRad * 0.5));
  return RoadPageRestoration::DisplayView{.Eye = world.Start,
                                          .ViewportPx = viewport,
                                          .Projection = {.FocalPx = focal},
                                          .TranslationM = world.RadiusM,
                                          .PathEnd = world.End};
}

std::expected<void, std::string>
AdvanceRoadPlan(EngineServices &services,
                const Generators::Corridors::Site &site,
                const Around &coverage,
                WorldRegionBuildState &state,
                const std::shared_ptr<Generators::PreparedRoadPages> &assets,
                Core::DiagnosticLedger &published) {
  auto *pool = services.Compute.get();
  if (state.RoadPlanning() == nullptr) {
    if (pool == nullptr) { return std::unexpected("road planning has no worker pool"); }
    const auto began = std::chrono::steady_clock::now();
    state.BeginRoadPlanning(std::make_unique<RoadPlanTask>(
        *pool,
        RoadPlanTask::Input{.Sources = state.Candidate().SourceSnapshot(),
                            .Network = state.Candidate().Products().StreetGraph,
                            .Classes = site.Classes,
                            .Materials = site.Materials,
                            .SurfaceRecipes = site.Vegetation,
                            .Terrain = state.TerrainInputs(),
                            .Frame = site.Standing,
                            .Projection = site.Projection,
                            .Began = site.CensusAt,
                            .Eye = {.LongitudeDeg = site.EyeLonDeg, .LatitudeDeg = site.EyeLatDeg},
                            .SheetZoom = coverage.Zoom,
                            .DemZoom = site.Ground->BlockZoom()}));
    published.RecordMetric(
        "ground candidate: corridor job admission",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count(),
        "ms");
    return {};
  }
  auto result = state.RoadPlanning()->Take();
  if (!result) { return {}; }
  state.FinishRoadPlanning();
  if (!*result) { return std::unexpected(std::move(result->error())); }
  published.RecordMetric("ground candidate: road plan worker", (*result)->WorkerMs, "ms");
  published.RecordMetric(
      "ground candidate: road plan worker longest step", (*result)->LongestStepMs, "ms");
  state.BeginsCorridors(std::move((*result)->Plan));
  for (const auto &one : Generators::Corridors::PlanMeasures(*state.CorridorJob())) {
    published.RecordMetric(one.Name, one.Value, one.Unit.c_str());
  }
  auto contacts = Generators::Corridors::TakeTerrainContacts(*state.CorridorJob());
  if (!contacts) { return std::unexpected(std::string(contacts.error())); }
  if (assets && pool != nullptr && !state.RegionKey().empty()) {
    auto pages = RoadPageConstruction::Begin(
        *pool, assets, *state.CorridorJob(), site, state.RegionKey(), services.AssetPublications());
    if (!pages) { return std::unexpected(std::move(pages.error())); }
    state.BeginRoadPages(std::move(*pages));
  }
  state.HoldsCorridors(std::move(*contacts));
  state.MarkCorridorsPlanned();
  return {};
}

std::string TerrainMissMessage(const std::optional<Drape::EastNorth> &first,
                               size_t misses,
                               const Around &coverage,
                               int demZoom,
                               const Generators::Osm::RegionSources &sources) {
  constexpr auto reason = "corridor generation reached terrain without a DEM field";
  if (!first) { return reason; }
  const auto *shapes = sources.Vectors.get();
  const int vectorZoom =
      shapes != nullptr && !shapes->Tiles().empty() ? shapes->Tiles().front().Z : -1;
  return std::format(
      "{}: east {:.3f} m, north {:.3f} m, {} misses, sheet zoom {}, DEM zoom {}, OSM zoom {}",
      reason,
      first->EastM,
      first->NorthM,
      misses,
      coverage.Zoom,
      demZoom,
      vectorZoom);
}

std::expected<void, std::string> FinishRoadPreparation(WorldRegionBuildState &state,
                                                       Core::DiagnosticLedger &published) {
  auto *pages = state.RoadPages();
  for (const auto &diagnostic : state.CorridorJob()->Diagnostics()) {
    published.RecordMetric(diagnostic.Name, diagnostic.Value, diagnostic.Unit.c_str());
  }
  published.RecordMetric("ground candidate: road plan", state.CorridorJob()->WorkMs(), "ms");
  published.RecordMetric("ground candidate: native road miss requests",
                         static_cast<double>(pages->MissRequests()),
                         "pages");
  published.RecordMetric(
      "ground candidate: native road pages", static_cast<double>(pages->Pages()), "pages");
  published.RecordMetric("ground candidate: native road page bytes",
                         static_cast<double>(pages->NativeBytes()),
                         "bytes");
  auto manifest = pages->TakeManifest();
  if (!manifest) { return std::unexpected(std::move(manifest.error())); }
  state.HoldRoadManifest(
      std::make_shared<const Generators::RoadPageManifest>(std::move(*manifest)));
  state.FinishRoadPages();
  return {};
}

std::expected<bool, std::string> AdvanceRoadProducts(const Generators::Corridors &generator,
                                                     WorldRegionBuildState &state,
                                                     const Generators::Corridors::Site &site,
                                                     Geometry &ground,
                                                     std::vector<EarthworkStamp> &contacts,
                                                     std::vector<DiagnosticSample> &notes) {
  if (auto *pages = state.RoadPages()) { return pages->Advance(*state.CorridorJob(), site); }
  const auto ready = generator.Advance(*state.CorridorJob(),
                                       site,
                                       kCorridorLanesPerFrame,
                                       kCorridorNodesPerFrame,
                                       ground,
                                       contacts,
                                       notes);
  if (!ready) { return std::unexpected(std::string(ready.error())); }
  return *ready;
}
}

bool Engine::State::BuildGroundCorridors(const TangentFrame &standing,
                                         const Around &coverage,
                                         WorldRegionBuildState &state) {
  const auto began = std::chrono::steady_clock::now();
  if (state.RetiringCorridors()) {
    if (!state.AdvancesCorridorRetirement(began)) { return true; }
    state.AdvanceStage();
    Published.RecordMetric(
        "ground candidate: corridor job retirement", state.LongestCorridorRetirementMs(), "ms");
    Published.RecordMetric(
        "ground candidate: longest corridor slice", state.LongestCorridorSliceMs(), "ms");
    return true;
  }
  GroundBuildProducts &build = state.Candidate().Products();
  const TriangleBvh surface;
  Published.RecordMetric("ground: triangles the drape can reach",
                         static_cast<double>(build.Indices.size()) / 3.0,
                         "triangles");
  Drape drapedOver{.Surface = surface, .Field = {}};
  size_t fieldMisses = 0;
  std::optional<Drape::EastNorth> firstFieldMiss;
  drapedOver.Field = [&coverage, &state, &fieldMisses, &firstFieldMiss](Drape::EastNorth at) {
    const std::optional<double> sampled = state.TerrainInputs().FieldUpM(coverage.Zoom, at);
    fieldMisses += sampled ? 0u : 1u;
    if (!sampled && !firstFieldMiss) { firstFieldMiss = at; }
    return sampled;
  };
  std::vector<EarthworkStamp> corridors;
  std::vector<DiagnosticSample> notes;
  const ::outshine::Generators::Osm::RegionSources &sources = state.Candidate().Sources();
  const Generators::Corridors::Site site{
      .Vectors = sources.Vectors.get(),
      .Ways = sources.Ways,
      .Materials = World.Surface().Materials(),
      .Vegetation = World.Surface().SurfaceRecipes(),
      .Ground = &World.Surface().Ground(),
      .Network = build.StreetGraph.get(),
      .Standing = standing,
      .Draped = drapedOver,
      .Classes = build.TerrainClassificationMap,
      .WaterUpM = [&sources, &standing](LongitudeLatitude at) -> std::optional<double> {
        return Generators::WaterSurfaceUpAt(sources.WaterBodies, standing, at);
      },
      .CensusAt = state.Began(),
      .EyeLatDeg = coverage.LatitudeDeg,
      .EyeLonDeg = coverage.LongitudeDeg,
      .Projection = build.Footprints.Projection()};
  if (!state.CorridorsPlanned()) {
    const auto plan =
        AdvanceRoadPlan(Services, site, coverage, state, World.Surface().RoadAssets(), Published);
    if (!plan) {
      Error = plan.error();
      return false;
    }
    state.SamplesCorridorSlice(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
            .count());
    return true;
  }
  const auto paved =
      AdvanceRoadProducts(World.Builtins.Corridors(), state, site, build.Ground, corridors, notes);
  state.SamplesProductPeak();
  if (!paved) {
    Error = paved.error();
    return false;
  }
  if (fieldMisses > 0) {
    Error = TerrainMissMessage(
        firstFieldMiss, fieldMisses, coverage, World.Surface().Ground().BlockZoom(), sources);
    return false;
  }
  if (!*paved) {
    state.SamplesCorridorSlice(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
            .count());
    return true;
  }
  for (const DiagnosticSample &one : notes) {
    Published.RecordMetric(one.Name, one.Value, one.Unit.c_str());
  }
  Published.RecordMetric(
      "ground candidate: corridor drape field misses", static_cast<double>(fieldMisses), "queries");
  if (state.RoadPages() != nullptr) {
    const auto finished = FinishRoadPreparation(state, Published);
    if (!finished) {
      Error = finished.error();
      return false;
    }
  } else {
    Published.RecordMetric("ground candidate: corridors", state.CorridorJob()->WorkMs(), "ms");
  }
  state.BeginsCorridorRetirement();
  state.SamplesCorridorSlice(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
  return true;
}

Engine::State::GroundBuildProgress
Engine::State::RestoreGroundRoadPages(WorldRegionBuildState &state) {
  if (state.RoadsRestored()) { return GroundBuildProgress::Ready; }
  if (!state.RoadManifest()) {
    state.Candidate().Products().Roads.Clear();
    state.FinishRoadRestoration();
    return GroundBuildProgress::Ready;
  }
  if (state.RoadRestoration() == nullptr) {
    if (!Services.Compute) {
      Error = "road restoration has no worker pool";
      return GroundBuildProgress::Failed;
    }
    if (const auto &index = state.RoadManifest()->PageIndex) {
      Published.RecordMetric("ground candidate: road page hierarchy nodes",
                             static_cast<double>(index->Hierarchy.Nodes().size()),
                             "nodes");
      Published.RecordMetric("ground candidate: road page hierarchy bytes",
                             static_cast<double>(index->ByteCount),
                             "bytes");
      Published.RecordMetric("ground candidate: road page hierarchy cache hit",
                             index->From == PageIndexAsset::Origin::Stored ? 1.0 : 0.0,
                             "bool");
    }
    const auto origin = Session.Declared.Ground.Origin;
    const auto frame =
        TangentFrame::At({.LongitudeDeg = origin.LongitudeDeg, .LatitudeDeg = origin.LatitudeDeg});
    auto publication = RoadGeometryPublication::Begin(
        Picture.Device, state.Candidate().Products().Roads, *state.RoadManifest(), frame);
    if (!publication) {
      Error = std::move(publication.error());
      return GroundBuildProgress::Failed;
    }
    std::optional<RoadPageRestoration::DisplayView> display;
    if (const auto &sweep = state.Coverage().Sweep; sweep && sweep->EyePath && Picture.Standing) {
      const auto &camera =
          Picture.Standing->Watched() ? Picture.Standing->Watching() : Picture.Standing->Aimed();
      display = RoadDisplayView(*sweep->EyePath,
                                frame,
                                camera,
                                {{static_cast<double>(Picture.Frame.WidthPx),
                                  static_cast<double>(Picture.Frame.HeightPx)}});
    }
    auto restore = RoadPageRestoration::Begin(*Services.Compute,
                                              World.Surface().RoadAssets(),
                                              state.RoadManifest(),
                                              (*publication)->PagesToLoad(),
                                              state.SurfaceManifestKey(),
                                              CurrentGeographicEyeEcef(),
                                              RoadPageRestoration::DefaultLimits,
                                              Services.AssetPublications(),
                                              display);
    if (!restore) {
      Error = std::move(restore.error());
      return GroundBuildProgress::Failed;
    }
    state.BeginRoadRestoration(std::move(*restore), std::move(*publication));
    return GroundBuildProgress::Pending;
  }
  auto *restore = state.RoadRestoration();
  auto *publication = state.RoadPublication();
  const auto advanced =
      restore->Advance([publication](const auto &page, const auto &reference, auto until) {
        return publication->Advance(page, reference, until);
      });
  state.SamplesProductPeak();
  if (!advanced) {
    Error = advanced.error();
    return GroundBuildProgress::Failed;
  }
  if (!*advanced) { return GroundBuildProgress::Pending; }
  Published.RecordMetric(
      "ground candidate: restored road pages", static_cast<double>(restore->Pages()), "pages");
  Published.RecordMetric("ground candidate: recovered road pages",
                         static_cast<double>(restore->RecoveredPages()),
                         "pages");
  Published.RecordMetric("ground candidate: road display pages",
                         static_cast<double>(restore->DisplayPages()),
                         "pages");
  Published.RecordMetric("ground candidate: retained resident road pages",
                         static_cast<double>(publication->RetainedPages()),
                         "pages");
  Published.RecordMetric("ground candidate: restored road package bytes",
                         static_cast<double>(restore->NativeBytes()),
                         "bytes");
  Published.RecordMetric("ground candidate: swept pieces in restored roads",
                         static_cast<double>(restore->Meshing().Pieces),
                         "pieces");
  Published.RecordMetric("ground candidate: refused pieces in restored roads",
                         static_cast<double>(restore->Meshing().Refused),
                         "pieces");
  const auto finished = publication->Finish();
  if (!finished) {
    Error = finished.error();
    return GroundBuildProgress::Failed;
  }
  Published.RecordMetric("ground candidate: road publication staging bytes",
                         static_cast<double>(publication->HeapBytes()),
                         "bytes");
  Published.RecordMetric("ground candidate: road CPU packing", publication->PackingMs(), "ms");
  Published.RecordMetric(
      "ground candidate: road CPU publication", publication->PublicationMs(), "ms");
  state.FinishRoadRestoration();
  return GroundBuildProgress::Pending;
}

}
