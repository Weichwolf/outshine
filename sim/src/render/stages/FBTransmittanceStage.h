/* Atmosphere LUT 1 of 2. Compute-only and needs no per-frame uniform: the transmittance LUT is
 * parametrised purely by height and sun-cos-theta. TODO cache it while the sun is static. */
#ifndef FBTRANSMITTANCESTAGE_H
#define FBTRANSMITTANCESTAGE_H

#include "FBDrawStage.h"

namespace FlightBox::Render {

class FBTransmittanceStage : public FBDrawStage {
public:
  /* The texture stays FBRenderer-owned (several stages read it); this holds only the write side. */
  void Configure(const FBGpu &gpu, wgpu::TextureView transLutView);
  void EncodeCompute(const FBFrameContext &ctx, wgpu::ComputePassEncoder &pass) override;

private:
  wgpu::ComputePipeline Pipe;
  wgpu::BindGroup Bind;
};

} // namespace FlightBox::Render
#endif
