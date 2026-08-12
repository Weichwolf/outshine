/* Atmosphere LUT 1 of 2. Compute-only and needs no per-frame uniform: the transmittance LUT is
 * parametrised purely by height and sun-cos-theta. TODO cache it while the sun is static. */
#ifndef TRANSMITTANCESTAGE_H
#define TRANSMITTANCESTAGE_H

#include "DrawStage.h"

namespace outshine::Render {

class TransmittanceStage : public DrawStage {
public:
  /* The texture stays Renderer-owned (several stages read it); this holds only the write side. */
  void Configure(const Gpu &gpu, wgpu::TextureView transLutView);
  void EncodeCompute(const FrameContext &ctx, wgpu::ComputePassEncoder &pass) override;

private:
  wgpu::ComputePipeline Pipe;
  wgpu::BindGroup Bind;
};

} // namespace outshine::Render
#endif
