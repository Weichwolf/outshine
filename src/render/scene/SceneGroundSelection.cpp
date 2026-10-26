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
  return state.Content.Geometry.Ground().ReadDrawArguments();
}

bool SceneRenderer::ConfigureGroundPyramid([[maybe_unused]] SceneRenderer &renderer,
                                           FrameResources &frame,
                                           [[maybe_unused]] const ExecutionPlan &plan,
                                           [[maybe_unused]] bool drawsGlass,
                                           std::string &error) {
  return Accepted(
      frame.GroundPyramid.Configure(frame.Targets.Get(Resource::SceneDepth), frame.Samp.get()),
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
  return ActiveFrame().GroundPyramid.Encode(into);
}

Native::Result<void> SceneRenderer::EncodeGroundSelection(const FrameContext &ctx,
                                                          const PassRecording &into) {
  const auto &subjects = ActiveState().Content.Geometry;
  const auto &frame = ActiveFrame();
  return ActiveState().Content.Geometry.Ground().SelectPatches(ctx,
                                                               subjects.AnchorM(),
                                                               Picture(true, into),
                                                               *frame.GroundPyramid.Buffer(),
                                                               frame.GroundPyramid.Shape());
}

Native::Result<void> SceneRenderer::EncodeGround(const FrameContext &ctx,
                                                 const PassRecording &into) {
  return ActiveState().Content.Geometry.EncodeGround(ctx, Picture(true, into));
}
}
