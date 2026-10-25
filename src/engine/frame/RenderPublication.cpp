#include "EngineState.h"
#include <chrono>
#include <cmath>
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
      "subject draws", static_cast<double>(Picture.Device.GeometryDrawCount()), "draws");
  Published.RecordMetric("the geometry the renderer was last offered, digested",
                         Picture.Standing->TransferMetrics().DigestValue(),
                         "");
  Published.RecordMetric("subject residency upload attempts in all",
                         static_cast<double>(Picture.Device.TotalUploadAttempts()),
                         "uploads");
  Published.RecordMetric("staged crossings recorded in copy passes",
                         static_cast<double>(Picture.Device.RecordedCrossings()),
                         "crossings");
  Published.RecordMetric("subject clusters",
                         static_cast<double>(Picture.Standing->Shown().Clusters.size()),
                         "clusters");
  Published.RecordMetric("cull: jobs it swept",
                         static_cast<double>(Render::GeometryCullStage::JobsSweptTaken()),
                         "jobs");
  if (!ScenarioState->Entities.Bodies().empty()) {
    Published.RecordMetric("bodies standing on no route",
                           static_cast<double>(ScenarioState->Entities.Bodies().size()),
                           "bodies");
    Published.RecordMetric(
        "the first of them, up", ScenarioState->Entities.Bodies().front().Motion.PositionM[1], "m");
    Published.RecordMetric("and how fast it falls",
                           ScenarioState->Entities.Bodies().front().Motion.VelocityMs[1],
                           "m/s");
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
  {
    const Render::Viewpoint &eye = Picture.Standing->Aimed();
    const double aspect = Picture.Frame.HeightPx > 0
                              ? static_cast<double>(Picture.Frame.WidthPx) /
                                    static_cast<double>(Picture.Frame.HeightPx)
                              : 1.0;
    const double half = 0.5 * eye.YfovRad;
    const double up = std::tan(half);
    const double across = up * aspect;
    size_t kept = 0;
    for (const DagCluster &one : Picture.Standing->Shown().Clusters) {
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
    Published.RecordMetric(
        "ring: clusters a frustum would keep", static_cast<double>(kept), "clusters");
  }
  Published.RecordMetric(
      "subject draw calls", static_cast<double>(Picture.Device.GeometryDrawCallCount()), "calls");
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
