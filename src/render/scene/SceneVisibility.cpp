#include "SceneRenderer.h"
#include <expected>
#include <string>

namespace outshine::Render {
GeometryCullStage::SelectionProjection
SceneRenderer::GeometryProjection(const FrameContext &ctx) const {
  auto lens = Through();
  const bool temporal = ActiveState().Plan->Holds(Stage::TemporalResolve);
  lens.Jitter = {};
  return {.Clip = temporal ? lens.ViewProjection(ActiveState().Camera) : ctx.Mvp,
          .GuardNdc = temporal ? Vec2f{{1.0f / static_cast<float>(lens.WidePx),
                                        1.0f / static_cast<float>(lens.HighPx)}}
                               : Vec2f{}};
}

bool SceneRenderer::ConfigureVisibilityPyramid(SceneRenderer &renderer,
                                               FrameResources &frame,
                                               const ExecutionPlan &plan,
                                               bool drawsGlass,
                                               std::string &error) {
  (void)renderer;
  (void)plan;
  (void)drawsGlass;
  return Accepted(
      frame.VisibilityPyramid.Configure(frame.Targets.Get(Resource::SceneDepth), frame.Samp.get()),
      error);
}

Native::Result<void> SceneRenderer::EncodeVisibilityPyramid(const FrameContext &ctx,
                                                            const PassRecording &into) {
  (void)ctx;
  return ActiveFrame().VisibilityPyramid.Encode(into);
}

Native::Result<void> SceneRenderer::EncodeGeometryRetryRoots(const FrameContext &ctx,
                                                             const PassRecording &into) {
  auto &frame = ActiveFrame();
  frame.Cull.CurrentPyramidFrom(frame.VisibilityPyramid.Buffer(), frame.VisibilityPyramid.Shape());
  const auto encoded = frame.Cull.EncodeRoots(
      ctx, into, GeometryProjection(ctx), GeometryCullStage::Phase::Deferred);
  if (!encoded) { return encoded; }
  if (frame.Cull.JobsAdmitted() != 0) {
    ++Costs_.DeferredSelections;
    if (frame.Cull.EncodedOcclusion()) { ++Costs_.OcclusionSelections; }
  }
  return {};
}

Native::Result<void> SceneRenderer::EncodeGeometryRetry(const FrameContext &ctx,
                                                        const PassRecording &into) {
  return ActiveContent().Geometry().Encode(ctx, Picture(true, into), GeometryDrawPhase::Deferred);
}
}
