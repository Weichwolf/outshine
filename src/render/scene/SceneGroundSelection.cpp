#include "SceneRenderer.h"
#include "DepthPyramid.h"
#include <expected>
#include <cstdint>
#include <string>

namespace outshine::Render {
Native::Result<TerrainRenderer::DrawArguments> SceneRenderer::ReadGroundDrawArguments() const {
  const auto &state = ActiveState();
  if (!state.Ready || !state.Submitted || !state.Plan) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = "no submitted ground frame"});
  }
  if (!state.Plan->Holds(Stage::Ground)) { return TerrainRenderer::DrawArguments{}; }
  return state.Content.Geometry.Ground().ReadDrawArguments();
}

bool SceneRenderer::ConfigureGroundPyramid([[maybe_unused]] SceneRenderer &renderer,
                                           FrameResources &frame,
                                           [[maybe_unused]] const ExecutionPlan &plan,
                                           [[maybe_unused]] bool drawsGlass,
                                           std::string &error) {
  return Accepted(
      frame.GroundPyramidStage.Configure(frame.Handles,
                                         frame.DepthTex.get(),
                                         frame.Samp.get(),
                                         frame.GroundPyramid.get(),
                                         {.WidthPx = frame.Width, .HeightPx = frame.Height}),
      error);
}

bool SceneRenderer::ConfigureGroundSelection([[maybe_unused]] SceneRenderer &renderer,
                                             FrameResources &frame,
                                             [[maybe_unused]] const ExecutionPlan &plan,
                                             [[maybe_unused]] bool drawsGlass,
                                             std::string &error) {
  return Accepted(frame.GeometryPipelines.ConfigureGroundSelection(*frame.Handles.Device), error);
}

Native::Result<void> SceneRenderer::EncodeGroundPyramid([[maybe_unused]] const FrameContext &ctx,
                                                        const PassRecording &into) {
  if (ActiveState().Content.Geometry.Ground().Drawn() == 0) { return {}; }
  return ActiveFrame().GroundPyramidStage.Encode(into);
}

Native::Result<void> SceneRenderer::EncodeGroundSelection(const FrameContext &ctx,
                                                          const PassRecording &into) {
  const auto &subjects = ActiveState().Content.Geometry;
  const auto &frame = ActiveFrame();
  return ActiveState().Content.Geometry.Ground().SelectPatches(
      ctx,
      subjects.AnchorM(),
      Picture(true, into),
      *frame.GroundPyramid,
      PyramidOver({.WidthPx = static_cast<uint32_t>(frame.Width),
                   .HeightPx = static_cast<uint32_t>(frame.Height)}));
}

Native::Result<void> SceneRenderer::EncodeGround(const FrameContext &ctx,
                                                 const PassRecording &into) {
  return ActiveState().Content.Geometry.EncodeGround(ctx, Picture(true, into));
}
}
