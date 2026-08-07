/* ONE shader, ONE pipeline: the display curve from the HDR scene target to the frame target, plus the
 * ambient-occlusion composite. AO belongs HERE and not in a pass of its own because this shader
 * already reads every scene pixel, and because it is the only place that has both the radiance and
 * the direct fraction the surfaces wrote into the HDR alpha (stages/SurfaceLight.h).
 * The curve carries no constants of its own: ExposureStage meters them off the picture. */
#ifndef TONEMAPSTAGE_H
#define TONEMAPSTAGE_H

#include "DrawStage.h"

namespace outshine::Render {

class TonemapStage : public DrawStage {
public:
  /* Where the log-linear ramp hands over to the toe, as a fraction of the black-to-white span.
   * 0 is the hard clamp this replaces. */
  static constexpr double kToe = 0.0551;

  /* TWO scene views, because the temporal resolve ping-pongs its accumulation and a bind group pins
   * a texture view at creation. Without TAA both are the raw scene target and the parity is inert. */
  void Configure(const Gpu &gpu, wgpu::Sampler samp, wgpu::TextureView sceneEven,
                 wgpu::TextureView sceneOdd, wgpu::TextureView aoView, wgpu::Buffer meterBuf);
  void Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  wgpu::RenderPipeline TonemapPipe;
  wgpu::BindGroup TonemapBind[2];
};

} // namespace outshine::Render
#endif
