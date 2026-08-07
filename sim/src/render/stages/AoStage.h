/* Screen-space ambient occlusion off the scene depth, at half resolution into an R8. It is a
 * SEPARATE pass because it samples the depth texture that was an attachment a moment ago; the
 * composite is not a pass at all — TonemapStage already reads the HDR target and simply reads this
 * one too, weighted by the direct fraction the surfaces wrote into the HDR alpha (SurfaceLight.h).
 *
 * Half resolution and a linear sampler on the way out: the AO field has no detail above ~1 m at the
 * radius used here, and the bilinear upsample is what removes the sample noise a per-pixel spiral
 * would otherwise leave. */
#ifndef AOSTAGE_H
#define AOSTAGE_H

#include "DrawStage.h"

namespace outshine::Render {

class AoStage : public DrawStage {
public:
  void Configure(const Gpu &gpu, wgpu::TextureView depthView, wgpu::Buffer atmoBuf, int width,
                 int height);
  void Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

  wgpu::TextureView OutputView(void) const { return Out.CreateView(); }
  int OutWidth(void) const { return W; }
  int OutHeight(void) const { return H; }

private:
  wgpu::Device Device;
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
  wgpu::Texture Out;
  bool Enabled = true;
  int W = 0, H = 0;
};

} // namespace outshine::Render
#endif
