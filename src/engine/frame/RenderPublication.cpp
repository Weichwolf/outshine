#include "EngineState.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <ratio>

namespace outshine {

namespace {

constexpr std::array<const char *, static_cast<size_t>(Render::RenderFramePhase::Count)>
    kSlowestRenderPhaseNames = {"render host slowest frame: prepare",
                                "render host slowest frame: acquire",
                                "render host slowest frame: upload",
                                "render host slowest frame: swapchain",
                                "render host slowest frame: cull",
                                "render host slowest frame: encode",
                                "render host slowest frame: fence wait",
                                "render host slowest frame: submit",
                                "render host slowest frame: finish"};

void PublishRenderFrameTiming(Core::DiagnosticLedger &published,
                              const Render::SceneRenderer &device) {
  const Render::RenderFrameTiming &slowest = device.SlowestRenderFrameTiming();
  published.RecordMetric("render host slowest frame: total", slowest.TotalMs, "ms");
  for (size_t phase = 0; phase < kSlowestRenderPhaseNames.size(); ++phase) {
    published.RecordMetric(kSlowestRenderPhaseNames[phase], slowest.PhaseMs[phase], "ms");
  }
}

void PublishStageMeasurements(Core::DiagnosticLedger &published,
                              const Render::SceneRenderer &device) {
  static const auto names = [] {
    std::array<std::array<std::string, 11>, Render::kStageCount> out;
    constexpr std::array<const char *, 11> suffixes{", took",
                                                    ", encoded draw requests",
                                                    ", direct input triangles",
                                                    ", referenced surface slot limit",
                                                    ", referenced placement slot limit",
                                                    ", resident textured slots",
                                                    ", resident colour images",
                                                    ", owned device bytes",
                                                    ", resident vertex layouts",
                                                    ", indirect input triangles",
                                                    ", indirect draw records"};
    for (size_t at = 0; at < out.size(); ++at) {
      for (size_t key = 0; key < suffixes.size(); ++key) {
        out[at][key] = std::string(Row(static_cast<Render::Stage>(at)).Name) + suffixes[key];
      }
    }
    return out;
  }();
  for (size_t at = 0; at < Render::kStageCount; ++at) {
    const auto &spent = device.Spent(static_cast<Render::Stage>(at));
    if (spent.TookMs <= 0.0 && spent.Draws == 0) { continue; }
    const std::array<double, 11> values{spent.TookMs,
                                        static_cast<double>(spent.Draws),
                                        static_cast<double>(spent.DirectTriangles),
                                        static_cast<double>(spent.Surfaces),
                                        static_cast<double>(spent.Placements),
                                        static_cast<double>(spent.Textured),
                                        static_cast<double>(spent.Palettes),
                                        static_cast<double>(spent.DeviceBytes),
                                        static_cast<double>(spent.Layouts),
                                        static_cast<double>(spent.IndirectInputTriangles),
                                        static_cast<double>(spent.IndirectRecords)};
    constexpr std::array<const char *, 11> units{"ms",
                                                 "draws",
                                                 "triangles",
                                                 "slots",
                                                 "slots",
                                                 "slots",
                                                 "images",
                                                 "bytes",
                                                 "layouts",
                                                 "triangles",
                                                 "records"};
    published.RecordMetric(names[at][0], values[0], units[0]);
    const auto stage = static_cast<Render::Stage>(at);
    if (stage != Render::Stage::OpaqueGeometry && stage != Render::Stage::TransmissiveGeometry) {
      continue;
    }
    for (size_t key = 1; key < values.size(); ++key) {
      published.RecordMetric(names[at][key], values[key], units[key]);
    }
  }
}

}

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
  PublishStageMeasurements(Published, Picture.Device);
  if (Cost.Render.Taken() > 0) {
    Published.RecordMetric("the picture's own time, last", Cost.Render.LastMs(), "ms");
    Published.RecordMetric("the picture's own time, least", Cost.Render.LeastMs(), "ms");
    Published.RecordMetric("the picture's own time, most", Cost.Render.MostMs(), "ms");
    Published.RecordMetric("pictures drawn", static_cast<double>(Cost.Render.Taken()), "pictures");
    PublishRenderFrameTiming(Published, Picture.Device);
  }
  PublishCostMeasurements();
  Cost.Diagnostics.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - publicationAt)
          .count());
  Published.RecordMetric("cost.diagnostics.total_ms", Cost.Diagnostics.TotalMs(), "ms");
  Published.RecordMetric("cost.diagnostics.max_ms", Cost.Diagnostics.MostMs(), "ms");
}

}
