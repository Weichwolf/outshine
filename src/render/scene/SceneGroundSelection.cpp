#include "SceneRenderer.h"
#include "DepthPyramid.h"
#include <expected>
#include <cstdint>
#include <string>

namespace outshine::Render {
Native::Result<TerrainRenderer::DrawArguments> SceneRenderer::ReadGroundDrawArguments() const {
  const auto &state = ActiveState();
  if (!state.Ready || !state.History.HasSubmittedFrame() || !state.Plan) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = "no submitted ground frame"});
  }
  if (!state.Plan->Holds(Stage::Ground)) { return TerrainRenderer::DrawArguments{}; }
  return ActiveContent().Geometry().Ground().ReadDrawArguments();
}

bool SceneRenderer::ConfigureOpaqueDepthPyramid([[maybe_unused]] SceneRenderer &renderer,
                                                FrameResources &frame,
                                                [[maybe_unused]] const ExecutionPlan &plan,
                                                [[maybe_unused]] bool drawsGlass,
                                                std::string &error) {
  return Accepted(
      frame.OpaqueDepthPyramid.Configure(frame.Targets.Get(Resource::SceneDepth), frame.Samp.get()),
      error);
}

bool SceneRenderer::ConfigureGroundSelection([[maybe_unused]] SceneRenderer &renderer,
                                             FrameResources &frame,
                                             [[maybe_unused]] const ExecutionPlan &plan,
                                             [[maybe_unused]] bool drawsGlass,
                                             std::string &error) {
  return Accepted(frame.GeometryPipelines.ConfigureGroundSelection(*frame.Handles.Device), error);
}

bool SceneRenderer::ConfigureGround(SceneRenderer &renderer,
                                    FrameResources &frame,
                                    const ExecutionPlan &plan,
                                    [[maybe_unused]] bool drawsGlass,
                                    std::string &error) {
  auto gpu = frame.Handles;
  gpu.SceneColours = ColoursForStage(frame, plan, Stage::Ground);
  return Accepted(renderer.ActiveContent().Geometry().ConfigureGround(frame.GeometryPipelines, gpu),
                  error);
}

Native::Result<void>
SceneRenderer::EncodeOpaqueDepthPyramid([[maybe_unused]] const FrameContext &ctx,
                                        const PassRecording &into) {
  if (ActiveContent().Geometry().Ground().Drawn() == 0 &&
      !ActiveState().Plan->Holds(Stage::GeometryCompletionCull)) {
    return {};
  }
  return ActiveFrame().OpaqueDepthPyramid.Encode(into);
}

Native::Result<void> SceneRenderer::EncodeGroundSelection(const FrameContext &ctx,
                                                          const PassRecording &into) {
  const auto &geometry = ActiveContent().Geometry();
  const auto &frame = ActiveFrame();
  return ActiveContent().Geometry().Ground().SelectPatches(ctx,
                                                           geometry.AnchorM(),
                                                           Picture(true, into),
                                                           *frame.OpaqueDepthPyramid.Buffer(),
                                                           frame.OpaqueDepthPyramid.Shape());
}

Native::Result<void> SceneRenderer::EncodeGround(const FrameContext &ctx,
                                                 const PassRecording &into) {
  return ActiveContent().Geometry().EncodeGround(ctx, Picture(true, into));
}
}
