/* Atmosphere LUT 2 of 2: raymarches single scattering into SkyLUT, taking sun transmittance from
 * LUT 1. */
#ifndef FBSKYVIEWSTAGE_H
#define FBSKYVIEWSTAGE_H

#include "FBDrawStage.h"

namespace FlightBox::Render {

class FBSkyViewStage : public FBDrawStage {
public:
  /* `skyLutView` is this stage's write target; the other two are borrowed dependencies. */
  void Configure(const FBGpu &gpu, wgpu::TextureView skyLutView, wgpu::TextureView transLutView,
                wgpu::Sampler lutSamp, wgpu::Buffer atmoBuf);
  void EncodeCompute(const FBFrameContext &ctx, wgpu::ComputePassEncoder &pass) override;

private:
  wgpu::ComputePipeline Pipe;
  wgpu::BindGroup Bind;
};

} // namespace FlightBox::Render
#endif
