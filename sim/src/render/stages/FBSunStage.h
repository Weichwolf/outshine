/* FlightBox — FBSunStage: the sun disc + forward glow, split out of FBSkyStage's kSkyWGSL into its
 * own ADDITIVE draw (One/One blend) — same scene pass, encoded directly after FBSkyStage so blend
 * order matches the original single-shader composite exactly (pure addition is order-independent
 * anyway). Zero footprint outside its own contribution: returns vec4f(0) whenever EVS is off, so the
 * additive blend adds nothing over the sky/terrain beneath. */
#ifndef FBSUNSTAGE_H
#define FBSUNSTAGE_H

#include "FBDrawStage.h"

namespace FlightBox {

class FBSunStage : public FBDrawStage {
public:
  /* `transLutView` = the transmittance LUT (solar colour at grazing angles); both borrowed, FBRenderer-
   * owned atmosphere resources. */
  void Configure(const FBGpu &gpu, wgpu::Buffer atmoBuf, wgpu::Sampler lutSamp, wgpu::TextureView transLutView);
  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
};

} // namespace FlightBox
#endif
