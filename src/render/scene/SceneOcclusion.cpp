#include "SceneRenderer.h"
#include <expected>

namespace outshine::Render {
bool SceneRenderer::ConfigureOcclusion(SceneRenderer &renderer,
                                       FrameResources &frame,
                                       const ExecutionPlan &plan,
                                       bool drawsGlass,
                                       std::string &error) {
  (void)renderer;
  (void)plan;
  (void)drawsGlass;
  return Accepted(frame.Occlusion.Configure(*frame.Handles.Device,
                                            frame.Targets.Get(Resource::OccluderDepth),
                                            frame.Samp.get()),
                  error);
}

Native::Result<void> SceneRenderer::EncodeOccluders(const FrameContext &ctx,
                                                    const PassRecording &into) {
  auto &geometry = ActiveContent().Geometry();
  return ActiveFrame().Occlusion.EncodeDepth(
      geometry,
      ctx,
      into,
      ActiveFrame().Cull.HasSelectionFor(geometry.Resident(), geometry.Generation()));
}

Native::Result<void> SceneRenderer::EncodeOccluderPyramid(const FrameContext &ctx,
                                                          const PassRecording &into) {
  (void)ctx;
  return ActiveFrame().Occlusion.EncodePyramid(into);
}
}
