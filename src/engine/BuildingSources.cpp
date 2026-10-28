#include "EngineState.h"
#include "BuildingSourceDemand.h"
#include <memory>

namespace outshine {
void Engine::State::AdvanceBuildingSources() {
  if (!World.Stack.Opened()) { return; }
  if (!World.BuildingSources) { World.BuildingSources = std::make_unique<BuildingSourceDemand>(); }
  if (!World.BuildingSources->Advance(
          World.StructureBuilds, World.Stack, StructureBuildQueue::kCandidateWindow)) {
    return;
  }
  const auto sources = World.BuildingSources->Counts();
  Published.RecordMetric("building sources: ready", static_cast<double>(sources.Ready), "blocks");
  Published.RecordMetric(
      "building sources: pending", static_cast<double>(sources.Pending), "blocks");
  Published.RecordMetric(
      "building sources: missing", static_cast<double>(sources.Missing), "blocks");
  Published.RecordMetric("building sources: failed", static_cast<double>(sources.Failed), "blocks");
  Published.RecordMetric("building sources: metadata",
                         static_cast<double>(World.BuildingSources->HeapBytes()),
                         "bytes");
}
}
