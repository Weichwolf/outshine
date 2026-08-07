/* Atmosphere LUT 3 of 3: raymarches scattering into SkyLUT, taking sun transmittance from LUT 1 and
 * the multiple-scattering term from LUT 2. */
#ifndef SKYVIEWSTAGE_H
#define SKYVIEWSTAGE_H

#include "DrawStage.h"

namespace outshine::Render {

class SkyViewStage : public DrawStage {
public:
  /* `skyLutView` is this stage's write target; the rest are borrowed dependencies. */
  void Configure(const Gpu &gpu, wgpu::TextureView skyLutView, wgpu::TextureView transLutView,
                wgpu::Sampler lutSamp, wgpu::Buffer atmoBuf, wgpu::TextureView msLutView);
  void EncodeCompute(const FrameContext &ctx, wgpu::ComputePassEncoder &pass) override;

private:
  wgpu::ComputePipeline Pipe;
  wgpu::BindGroup Bind;
};

} // namespace outshine::Render
#endif
