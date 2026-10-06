#include "EngineHeld.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <ratio>
#include <string>

namespace outshine {
namespace {

template <size_t Count, size_t Keys, class Name>
auto MetricNames(const char *prefix, const std::array<const char *, Keys> &keys, Name name) {
  std::array<std::array<std::string, Keys>, Count> out;
  for (size_t at = 0; at < Count; ++at) {
    for (size_t key = 0; key < Keys; ++key) {
      out[at][key] = std::string(prefix) + name(at) + keys[key];
    }
  }
  return out;
}

void PublishPreparedTerrainCosts(Core::DiagnosticLedger &published,
                                 const Generators::PreparedTerrainAssets::Counters &prepared) {
  published.RecordMetric("cost.assets.terrain.hits", static_cast<double>(prepared.Hits), "reads");
  published.RecordMetric(
      "cost.assets.terrain.resident", static_cast<double>(prepared.Resident), "reads");
  published.RecordMetric(
      "cost.assets.terrain.misses", static_cast<double>(prepared.Misses), "reads");
  published.RecordMetric(
      "cost.assets.terrain.writes", static_cast<double>(prepared.Writes), "packages");
  published.RecordMetric(
      "cost.assets.terrain.read_bytes", static_cast<double>(prepared.ReadBytes), "bytes");
}

}

void Engine::State::PublishCostMeasurements() {
  const auto began = std::chrono::steady_clock::now();
  const auto store = World.Stack.StoreCosts();
  PublishPreparedTerrainCosts(Published, World.Stack.PreparedTerrainCosts());
  Published.RecordMetric("cost.cache.hits", static_cast<double>(store.Hits), "reads");
  Published.RecordMetric("cost.cache.misses", static_cast<double>(store.Misses), "reads");
  Published.RecordMetric("cost.cache.read_ms", store.ReadMs, "ms");
  Published.RecordMetric("cost.cache.write_ms", store.WriteMs, "ms");
  Published.RecordMetric("cost.cache.read_bytes", static_cast<double>(store.ReadBytes), "bytes");
  Published.RecordMetric("cost.cache.write_bytes", static_cast<double>(store.WriteBytes), "bytes");
  Published.RecordMetric("cost.cache.read_calls", static_cast<double>(store.ReadCalls), "calls");
  const auto providers = World.Stack.ProviderCosts();
  static const auto providerNames = MetricNames<Data::SourceSet::Ledger::KindCount>(
      "cost.provider.", std::array{".call_ms", ".calls"}, [](size_t at) {
        return Data::Name(static_cast<Data::DataKind>(at));
      });
  for (size_t at = 0; at < providers.size(); ++at) {
    Published.RecordMetric(providerNames[at][0], providers[at].Ms, "ms");
    Published.RecordMetric(providerNames[at][1], static_cast<double>(providers[at].Calls), "calls");
  }
  const auto jobs =
      World.Stack.Opened() ? World.Stack.Pool().Counters() : Ground::TilePool::Ledger{};
  Published.RecordMetric(
      "cost.cache.decoded_hits", static_cast<double>(jobs.Decoded.Hits), "reads");
  Published.RecordMetric(
      "cost.cache.decoded_reads", static_cast<double>(jobs.Decoded.Reads), "reads");
  Published.RecordMetric(
      "cost.cache.decoded_evictions", static_cast<double>(jobs.Decoded.Evictions), "rasters");
  Published.RecordMetric("cost.field.work_ms", jobs.FieldCpuMs, "ms");
  Published.RecordMetric(
      "cost.field.attempts", static_cast<double>(jobs.FieldAttempts), "attempts");
  Published.RecordMetric("cost.mesh.work_ms", jobs.MeshCpuMs, "ms");
  Published.RecordMetric("cost.mesh.attempts", static_cast<double>(jobs.MeshAttempts), "attempts");
  Published.RecordMetric("cost.fetch.service_ms", jobs.FetchMs, "ms");
  Published.RecordMetric("cost.fetch.blocked_ms", jobs.FetchBlockedMs, "ms");
  if (const auto *vectors = World.Stack.Vectors()) {
    const auto cost = vectors->TotalBuildMetrics();
    Published.RecordMetric("cost.osm.fetch_ms", cost.FetchMs, "ms");
    Published.RecordMetric("cost.osm.parse_ms", cost.ParseMs, "ms");
    Published.RecordMetric("cost.osm.capacity_ms", cost.CapacityMs, "ms");
    Published.RecordMetric("cost.osm.publish_ms", cost.PublicationMs, "ms");
  }
  const auto &structures = World.StructureBuilds;
  Published.RecordMetric("cost.structures.work_ms", structures.BakeWorkMs(), "ms");
  Published.RecordMetric(
      "cost.structures.products", static_cast<double>(structures.Landed()), "products");
  Published.RecordMetric(
      "cost.structures.ranges", static_cast<double>(structures.CompletedRanges()), "ranges");
  Published.RecordMetric("cost.structures.max_range_ms", structures.SlowestRangeMs(), "ms");
  static const auto groundNames = MetricNames<Spent::kGroundPhaseCount>(
      "cost.ground.", std::array{".total_ms", ".max_ms", ".calls"}, [](size_t at) {
        constexpr std::array names{"candidate",
                                   "patchwork",
                                   "fields",
                                   "refinement",
                                   "halos",
                                   "sheetmesh",
                                   "classes",
                                   "surface",
                                   "models",
                                   "network",
                                   "alignments",
                                   "corridors",
                                   "structures",
                                   "earthworks",
                                   "terrainmesh",
                                   "water",
                                   "geometry",
                                   "publication"};
        return names[at];
      });
  for (size_t at = 0; at < Cost.GroundPhases.size(); ++at) {
    const auto &cost = Cost.GroundPhases[at];
    Published.RecordMetric(groundNames[at][0], cost.TotalMs(), "ms");
    Published.RecordMetric(groundNames[at][1], cost.MostMs(), "ms");
    Published.RecordMetric(groundNames[at][2], static_cast<double>(cost.Taken()), "calls");
  }
  Published.RecordMetric("cost.diagnostics.profile_ms", Cost.ProfileDiagnostics.TotalMs(), "ms");
  if (!Picture.Standing) {
    Cost.ProfileDiagnostics.Took(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
            .count());
    return;
  }
  const auto &render = Picture.Device.Costs();
  Published.RecordMetric("cost.render.frames", static_cast<double>(render.Frames), "frames");
  static const auto hostNames = MetricNames<static_cast<size_t>(Render::RenderFramePhase::Count)>(
      "cost.host.", std::array{".total_ms", ".worst_ms"}, [](size_t at) {
        constexpr std::array names{"prepare",
                                   "acquire",
                                   "upload",
                                   "swapchain",
                                   "cull",
                                   "encode",
                                   "fence",
                                   "submit",
                                   "finish"};
        return names[at];
      });
  for (size_t at = 0; at < render.HostTotals.PhaseMs.size(); ++at) {
    Published.RecordMetric(hostNames[at][0], render.HostTotals.PhaseMs[at], "ms");
    Published.RecordMetric(
        hostNames[at][1], Picture.Device.SlowestRenderFrameTiming().PhaseMs[at], "ms");
  }
  static const auto stageNames = MetricNames<Render::kStageCount>(
      "cost.stage.", std::array{".total_ms", ".max_ms", ".frames"}, [](size_t at) {
        return Row(static_cast<Render::Stage>(at)).Name;
      });
  for (size_t at = 0; at < render.Stages.size(); ++at) {
    const auto &cost = render.Stages[at];
    Published.RecordMetric(stageNames[at][0], cost.TotalMs, "ms");
    Published.RecordMetric(stageNames[at][1], cost.MostMs, "ms");
    Published.RecordMetric(stageNames[at][2], static_cast<double>(cost.Frames), "frames");
  }
  Cost.ProfileDiagnostics.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
}

}
