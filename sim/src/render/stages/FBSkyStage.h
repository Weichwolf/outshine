/* The sky dome plus the cloud-deck noise sheet: the FIRST draw in the scene pass, depth Always and
 * no write, so the terrain draws over it. Sun and moon are their own additive draws right after. */
#ifndef FBSKYSTAGE_H
#define FBSKYSTAGE_H

#include "FBDrawStage.h"

namespace FlightBox {

class FBSkyStage : public FBDrawStage {
public:
  /* Borrowed atmosphere resources; this holds only the pipeline/bind group built from them. */
  void Configure(const FBGpu &gpu, wgpu::TextureView skyLutView, wgpu::Sampler lutSamp, wgpu::Buffer atmoBuf);
  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
};

} // namespace FlightBox
#endif
