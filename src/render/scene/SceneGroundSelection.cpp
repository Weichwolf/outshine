#include "SceneRenderer.h"
#include "DepthPyramid.h"
#include <expected>
#include <cstdint>
#include <string>

namespace outshine::Render {
bool SceneRenderer::ConfigureGroundPyramid([[maybe_unused]] SceneRenderer &renderer,
                                           FrameResources &frame,
                                           [[maybe_unused]] const Compiled &plan,
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

bool SceneRenderer::ConfigureGroundSelection(SceneRenderer &renderer,
                                             [[maybe_unused]] FrameResources &frame,
                                             [[maybe_unused]] const Compiled &plan,
                                             [[maybe_unused]] bool drawsGlass,
                                             std::string &error) {
  return Accepted(renderer.ActiveState().Content.Subjects.Ground().ConfigurePatchSelection(),
                  error);
}

Native::Result<void> SceneRenderer::EncodeGroundPyramid([[maybe_unused]] const FrameContext &ctx,
                                                        const PassRecording &into) {
  if (ActiveState().Content.Subjects.Ground().Drawn() == 0) { return {}; }
  return ActiveFrame().GroundPyramidStage.Encode(into);
}

Native::Result<void> SceneRenderer::EncodeGroundSelection(const FrameContext &ctx,
                                                          const PassRecording &into) {
  const auto &subjects = ActiveState().Content.Subjects;
  const auto &frame = ActiveFrame();
  return ActiveState().Content.Subjects.Ground().SelectPatches(
      ctx,
      subjects.AnchorM(),
      Picture(true, into),
      *frame.GroundPyramid,
      PyramidOver({.WidthPx = static_cast<uint32_t>(frame.Width),
                   .HeightPx = static_cast<uint32_t>(frame.Height)}));
}

Native::Result<void> SceneRenderer::EncodeGround(const FrameContext &ctx,
                                                 const PassRecording &into) {
  return ActiveState().Content.Subjects.EncodeGround(ctx, Picture(true, into));
}
}
