/* FlightBox — FBTransmittanceStage: Hillaire-2020 atmosphere, LUT 1 of 2. Compute-only, no per-frame
 * camera/sun uniform (the transmittance LUT is parametrised purely by height + sun-cos-theta) — owns
 * TransLUT and writes it fresh every frame (recomputed each frame; TODO cache while the sun is
 * static). FBSkyViewStage and FBSkyStage (and FBTilesStage's terrain aerial perspective) read it as a
 * dependency injected at their own Configure() — see FBRenderer::CreateAtmosphere for the Init-order
 * contract. */
#ifndef FBTRANSMITTANCESTAGE_H
#define FBTRANSMITTANCESTAGE_H

#include "FBDrawStage.h"

namespace FlightBox {

class FBTransmittanceStage : public FBDrawStage {
public:
  /* `transLutView` is FBRenderer-owned (TransLUT texture — a shared atmosphere resource multiple
   * stages read); this stage only holds the write-side pipeline/bind group for it. */
  void Configure(const FBGpu &gpu, wgpu::TextureView transLutView);
  void EncodeCompute(const FBFrameContext &ctx, wgpu::ComputePassEncoder &pass) override;

private:
  wgpu::ComputePipeline Pipe;
  wgpu::BindGroup Bind;
};

} // namespace FlightBox
#endif
