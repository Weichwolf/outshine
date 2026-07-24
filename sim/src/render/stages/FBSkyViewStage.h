/* FlightBox — FBSkyViewStage: Hillaire-2020 atmosphere, LUT 2 of 2. Raymarches single scattering
 * (sun transmittance read from FBTransmittanceStage's LUT) into SkyLUT, driven by the shared
 * per-frame Atmo uniform (camera/sun basis — FBRenderer::UpdateAtmosphere writes it). FBSkyStage
 * reads SkyLUT as a dependency injected at its own Configure(). */
#ifndef FBSKYVIEWSTAGE_H
#define FBSKYVIEWSTAGE_H

#include "FBDrawStage.h"

namespace FlightBox {

class FBSkyViewStage : public FBDrawStage {
public:
  /* `skyLutView` is this stage's own write target (FBRenderer-owned SkyLUT texture); `transLutView`
   * and `atmoBuf` are borrowed dependencies from FBTransmittanceStage's LUT and the shared per-frame
   * atmosphere uniform. */
  void Configure(const FBGpu &gpu, wgpu::TextureView skyLutView, wgpu::TextureView transLutView,
                wgpu::Sampler lutSamp, wgpu::Buffer atmoBuf);
  void EncodeCompute(const FBFrameContext &ctx, wgpu::ComputePassEncoder &pass) override;

private:
  wgpu::ComputePipeline Pipe;
  wgpu::BindGroup Bind;
};

} // namespace FlightBox
#endif
