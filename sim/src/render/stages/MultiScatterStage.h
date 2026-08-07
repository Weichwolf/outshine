/* Atmosphere LUT 2 of 3 (Hillaire 2020, eq. 5-7). Without it the sky-view march is single-scatter
 * only, which measures 1.4 EV too dark at the zenith and leaves ground and sky on two different
 * scales however the ground is fitted. Compute-only, parametrised by height and sun-cos-theta like
 * the transmittance LUT it reads. */
#ifndef MULTISCATTERSTAGE_H
#define MULTISCATTERSTAGE_H

#include "DrawStage.h"

namespace outshine::Render {

class MultiScatterStage : public DrawStage {
public:
  void Configure(const Gpu &gpu, wgpu::TextureView msLutView, wgpu::TextureView transLutView,
                 wgpu::Sampler lutSamp);
  void EncodeCompute(const FrameContext &ctx, wgpu::ComputePassEncoder &pass) override;

  static constexpr int kSide = 32;

private:
  wgpu::ComputePipeline Pipe;
  wgpu::BindGroup Bind;
};

} // namespace outshine::Render
#endif
