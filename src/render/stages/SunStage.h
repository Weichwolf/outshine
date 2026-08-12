/* The sun disc + forward glow as an ADDITIVE draw (One/One), encoded directly after SkyStage. */
#ifndef SUNSTAGE_H
#define SUNSTAGE_H

#include "DrawStage.h"

namespace outshine::Render {

class SunStage : public DrawStage {
public:
  /* The transmittance LUT gives the solar colour at grazing angles. Both borrowed. */
  void Configure(const Gpu &gpu, wgpu::Buffer atmoBuf, wgpu::Sampler lutSamp, wgpu::TextureView transLutView);
  void Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
};

} // namespace outshine::Render
#endif
