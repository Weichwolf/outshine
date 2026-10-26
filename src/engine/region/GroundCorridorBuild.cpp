#include "EngineState.h"
#include "WorldRegionBuildState.h"
#include "GroundWorldCandidate.h"
#include "DiagnosticLedger.h"
#include "WaterSurfaceBuilder.h"
#include <chrono>
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
std::shared_ptr<AssetPipeline> RoadPublications(WorldRegionState &world) {
  const auto pages = world.Stack.RoadAssets();
  if (!pages || !world.Pool) { return {}; }
  if (!world.Publications) {
    constexpr size_t kMebibyte = size_t{1024} * 1024;
    world.Publications = std::make_shared<AssetPipeline>(
        *world.Pool,
        AssetPipeline::Limits{.JobsMost = 4,
                              .ConsumersMost = 32,
                              .TicketsMost = 32,
                              .InputBytesMost = 128 * kMebibyte,
                              .ReceiptBytesMost = size_t{32} * 4096});
  }
  return world.Publications;
}

constexpr size_t kCorridorLanesPerFrame = 128;
constexpr size_t kCorridorNodesPerFrame = 64;

void BeginCorridorJob(WorldRegionBuildState &state,
                      const Generators::Corridors::Site &site,
                      Core::DiagnosticLedger &published) {
  const auto jobAt = std::chrono::steady_clock::now();
  state.BeginsCorridors(Generators::Corridors::Begin(site));
  published.RecordMetric(
      "ground candidate: corridor job admission",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - jobAt).count(),
      "ms");
  const auto inventoryAt = std::chrono::steady_clock::now();
  state.SamplesProductPeak();
  published.RecordMetric(
      "ground candidate: corridor product inventory",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inventoryAt)
          .count(),
      "ms");
}

std::expected<void, std::string>
AcceptRoadPlan(WorldRegionBuildState &state,
               const Generators::Corridors::Site &site,
               Tasks *pool,
               const std::shared_ptr<Generators::PreparedRoadPages> &assets,
               std::shared_ptr<AssetPipeline> publications) {
  auto contacts = Generators::Corridors::TakeTerrainContacts(*state.CorridorJob());
  if (!contacts) { return std::unexpected(std::string(contacts.error())); }
  if (assets && pool != nullptr && !state.RegionKey().empty()) {
    auto pages = RoadPageConstruction::Begin(
        *pool, assets, *state.CorridorJob(), site, state.RegionKey(), std::move(publications));
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
  const auto ready =
      !state.CorridorsPlanned()
          ? generator.AdvancePlan(
                *state.CorridorJob(), site, kCorridorLanesPerFrame, kCorridorNodesPerFrame)
          : generator.Advance(*state.CorridorJob(),
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
      .Materials = World.Stack.Materials(),
      .Vegetation = World.Stack.SurfaceRecipes(),
      .Ground = &World.Stack.Ground(),
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
  if (state.CorridorJob() == nullptr) {
    BeginCorridorJob(state, site, Published);
    state.SamplesCorridorSlice(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
            .count());
    return true;
  }
  const bool planning = !state.CorridorsPlanned();
  const auto paved =
      AdvanceRoadProducts(World.Shipping.Corridors(), state, site, build.Ground, corridors, notes);
  state.SamplesProductPeak();
  if (!paved) {
    Error = paved.error();
    return false;
  }
  if (fieldMisses > 0) {
    Error = TerrainMissMessage(
        firstFieldMiss, fieldMisses, coverage, World.Stack.Ground().BlockZoom(), sources);
    return false;
  }
  if (!*paved) {
    state.SamplesCorridorSlice(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
            .count());
    return true;
  }
  if (planning) {
    for (const auto &one : Generators::Corridors::PlanMeasures(*state.CorridorJob())) {
      Published.RecordMetric(one.Name, one.Value, one.Unit.c_str());
    }
    const auto accepted = AcceptRoadPlan(
        state, site, World.Pool.get(), World.Stack.RoadAssets(), RoadPublications(World));
    if (!accepted) {
      Error = accepted.error();
      return false;
    }
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
  if (state.RoadsRestored() || !state.RoadManifest()) { return GroundBuildProgress::Ready; }
  if (state.RoadRestoration() == nullptr) {
    if (!World.Pool) {
      Error = "road restoration has no worker pool";
      return GroundBuildProgress::Failed;
    }
    auto restore = RoadPageRestoration::Begin(*World.Pool,
                                              World.Stack.RoadAssets(),
                                              state.RoadManifest(),
                                              std::nullopt,
                                              state.SurfaceManifestKey());
    if (!restore) {
      Error = std::move(restore.error());
      return GroundBuildProgress::Failed;
    }
    state.BeginRoadRestoration(std::move(*restore));
    return GroundBuildProgress::Pending;
  }
  auto *restore = state.RoadRestoration();
  const auto advanced = restore->Advance(state.Candidate().Products().Ground);
  if (!advanced) {
    Error = advanced.error();
    return GroundBuildProgress::Failed;
  }
  if (!*advanced) { return GroundBuildProgress::Pending; }
  Published.RecordMetric(
      "ground candidate: restored road pages", static_cast<double>(restore->Pages()), "pages");
  Published.RecordMetric("ground candidate: restored road package bytes",
                         static_cast<double>(restore->NativeBytes()),
                         "bytes");
  Published.RecordMetric("streets: pieces the sweep laid on a line",
                         static_cast<double>(restore->Meshing().Pieces),
                         "pieces");
  Published.RecordMetric("streets: pieces the sweep could not lay",
                         static_cast<double>(restore->Meshing().Refused),
                         "pieces");
  state.FinishRoadRestoration();
  return GroundBuildProgress::Pending;
}

}
