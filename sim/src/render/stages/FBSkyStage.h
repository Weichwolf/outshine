/* FlightBox — FBSkyStage: the physically-based sky dome (+ the cloud-deck value-noise sheet), the
 * FIRST draw in the scene pass (depth Always/no-write, so terrain draws over it). Samples the sky-view
 * LUT (FBSkyViewStage's, injected at Configure()) and the shared per-frame Atmo uniform. The sun disc/
 * glow and the moon used to be baked into this same fragment shader (kSkyWGSL) — they are now their
 * own additive draws, FBSunStage and FBMoonStage, encoded directly after this one in the same pass
 * (same scene-pass blend order as the original single-shader composite). */
#ifndef FBSKYSTAGE_H
#define FBSKYSTAGE_H

#include "FBDrawStage.h"

namespace FlightBox {

class FBSkyStage : public FBDrawStage {
public:
  /* Both textures/buffer are borrowed (FBRenderer-owned atmosphere resources); this stage only holds
   * the render-side pipeline/bind group built from them. */
  void Configure(const FBGpu &gpu, wgpu::TextureView skyLutView, wgpu::Sampler lutSamp, wgpu::Buffer atmoBuf);
  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
};

} // namespace FlightBox
#endif
