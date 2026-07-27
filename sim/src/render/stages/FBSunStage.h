/* The sun disc + forward glow as an ADDITIVE draw (One/One), encoded directly after FBSkyStage.
 * Returns vec4f(0) whenever EVS is off, so the blend adds nothing at all. */
#ifndef FBSUNSTAGE_H
#define FBSUNSTAGE_H

#include "FBDrawStage.h"

namespace FlightBox::Render {

class FBSunStage : public FBDrawStage {
public:
  /* The transmittance LUT gives the solar colour at grazing angles. Both borrowed. */
  void Configure(const FBGpu &gpu, wgpu::Buffer atmoBuf, wgpu::Sampler lutSamp, wgpu::TextureView transLutView);
  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
};

} // namespace FlightBox::Render
#endif
