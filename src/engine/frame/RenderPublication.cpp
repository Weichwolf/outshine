#include "EngineState.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <ratio>

namespace outshine {

void Engine::State::PublishRenderedFrame(std::chrono::steady_clock::time_point began) {
  const auto published = Picture.Device.PublishedWorld();
  const auto publicationAt = std::chrono::steady_clock::now();
  Published.BeginFrame();
  PublishSubmittedCameraMeasurements();
  Cost.Render.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
  const Render::RenderFrameTiming &frameTiming = Picture.Device.LastRenderFrameTiming();
  Published.RecordMetric(
      "render host last submitted: fence wait",
      frameTiming.PhaseMs[static_cast<size_t>(Render::RenderFramePhase::FenceWait)],
      "ms");
  Published.RecordMetric(
      "geometry draw records", static_cast<double>(Picture.Device.GeometryDrawCount()), "draws");
  Published.RecordMetric("the geometry the renderer was last offered, digested",
                         Picture.Standing->TransferMetrics().DigestValue(),
                         "");
  Published.RecordMetric("geometry residency upload attempts total",
                         static_cast<double>(Picture.Device.TotalUploadAttempts()),
                         "uploads");
  Published.RecordMetric("staged crossings recorded in copy passes",
                         static_cast<double>(Picture.Device.RecordedCrossings()),
                         "crossings");
  Published.RecordMetric("geometry clusters",
                         static_cast<double>(Picture.Standing->Shown().Clusters.size()),
                         "clusters");
  Published.RecordMetric("cull: jobs it swept",
                         static_cast<double>(Render::GeometryCullStage::JobsSweptTaken()),
                         "jobs");
  const auto &bodyPublication = ScenarioState->Entities.PublicationCost();
  Published.RecordMetric("body publication: copied payload",
                         static_cast<double>(bodyPublication.CopiedBytes),
                         "bytes");
  Published.RecordMetric(
      "body publication: new pages", static_cast<double>(bodyPublication.NewPages), "pages");
  Published.RecordMetric("body publication: directory entries",
                         static_cast<double>(bodyPublication.DirectoryEntries),
                         "entries");
  if (BodyFrame.Count() != 0) {
    Published.RecordMetric(
        "bodies standing on no route", static_cast<double>(BodyFrame.Count()), "bodies");
    Published.RecordMetric("the first of them, up", BodyFrame.At(0).Motion.PositionM[1], "m");
    Published.RecordMetric("and how fast it falls", BodyFrame.At(0).Motion.VelocityMs[1], "m/s");
  }
  if (Session.Declared.Render.Audits) {
    Render::PyramidDepths depths;
    if (Picture.Standing->Pyramid(depths) == Render::ReadState::Ready) {
      Published.RecordMetric(
          "cull: the pyramid's nearest depth", static_cast<double>(depths.Nearest), "0..1");
      Published.RecordMetric("cull: its farthest", static_cast<double>(depths.Farthest), "0..1");
      Published.RecordMetric("cull: and its mean", static_cast<double>(depths.Mean), "0..1");
    }
  }
  Published.RecordMetric(
      "geometry draw calls", static_cast<double>(Picture.Device.GeometryDrawCallCount()), "calls");
  Published.RecordMetric(
      "plan passes", static_cast<double>(Picture.Standing->PlanPasses()), "passes");
  for (uint32_t at = 0; at < static_cast<uint32_t>(Render::kVertexLayouts.size()); ++at) {
    const uint32_t many =
        Picture.Device.GeometryBatchesTaking(static_cast<Render::VertexLayout>(at));
    if (many == 0) { continue; }
    Published.RecordMetric(
        "draws taking vertex layout " + std::to_string(at), static_cast<double>(many), "draws");
  }
  Drew();
  PublishCostMeasurements();
  Cost.Diagnostics.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - publicationAt)
          .count());
  Published.RecordMetric("cost.diagnostics.total_ms", Cost.Diagnostics.TotalMs(), "ms");
  Published.RecordMetric("cost.diagnostics.max_ms", Cost.Diagnostics.MostMs(), "ms");
}

}
