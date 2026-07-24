/* FlightBox — FBSkyStage: the physically-based sky dome, the FIRST draw in the scene pass (depth
 * Always/no-write, so terrain draws over it). Samples both atmosphere LUTs (FBTransmittanceStage's,
 * FBSkyViewStage's — both injected at Configure()), the shared per-frame Atmo uniform, and the NASA
 * moon albedo (a lit-sphere reconstruction). Sun disc/glow and the moon are still baked into this one
 * fragment shader today (kSkyWGSL) — splitting them into their own blended draws is a separate,
 * dedicated follow-up (real shader work + a pixel-proof), not part of this stage-extraction pass. */
#ifndef FBSKYSTAGE_H
#define FBSKYSTAGE_H

#include "FBDrawStage.h"

namespace FlightBox {

class FBSkyStage : public FBDrawStage {
public:
  /* All four textures/buffer are borrowed (FBRenderer-owned atmosphere resources); this stage only
   * holds the render-side pipeline/bind group built from them. */
  void Configure(const FBGpu &gpu, wgpu::TextureView skyLutView, wgpu::Sampler lutSamp,
                wgpu::TextureView transLutView, wgpu::Buffer atmoBuf, wgpu::TextureView moonTexView);
  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
};

} // namespace FlightBox
#endif
