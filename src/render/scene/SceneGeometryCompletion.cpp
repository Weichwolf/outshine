#include "SceneRenderer.h"

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

bool SceneRenderer::ConfigureGeometryCompletion(SceneRenderer &renderer,
                                                FrameResources &frame,
                                                const ExecutionPlan &plan,
                                                bool drawsGlass,
                                                std::string &error) {
  (void)plan;
  (void)drawsGlass;
  frame.Completion.PyramidFrom(frame.OpaqueDepthPyramid.Buffer(), frame.OpaqueDepthPyramid.Shape());
  return Accepted(
      frame.Completion.Configure(CullSource(renderer.ActiveContent().Geometry(),
                                            GeometryCullStage::SelectionKind::Completion),
                                 frame.Handles),
      error);
}

Native::Result<void> SceneRenderer::EncodeGeometryCompletionCull(const FrameContext &ctx,
                                                                 const PassRecording &into) {
  auto &stage = ActiveFrame().Completion;
  stage.Binds(CullSource(ActiveContent().Geometry(), GeometryCullStage::SelectionKind::Completion));
  stage.Projects(static_cast<float>(ActiveFrame().Height));
  return stage.EncodeCompletion(ctx, into, GeometryProjection(ctx));
}

Native::Result<void> SceneRenderer::EncodeGeometryCompletionScan(const FrameContext &ctx,
                                                                 const PassRecording &into) {
  (void)ctx;
  return ActiveFrame().Completion.EncodeScan(into);
}

Native::Result<void> SceneRenderer::EncodeGeometryCompletionCompact(const FrameContext &ctx,
                                                                    const PassRecording &into) {
  (void)ctx;
  const auto compact = ActiveFrame().Completion.EncodeCompact(into);
  if (!compact) { return compact; }
  return ActiveContent().Geometry().PrepareIndexedCommands(
      into, GeometryResidency::SelectionPhase::Completion);
}

Native::Result<void> SceneRenderer::EncodeOpaqueCompletion(const FrameContext &ctx,
                                                           const PassRecording &into) {
  return ActiveContent().Geometry().Encode(
      ctx, Picture(true, into), GeometryResidency::SelectionPhase::Completion);
}
}
