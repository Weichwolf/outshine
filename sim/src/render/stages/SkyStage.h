/* The sky dome plus the cloud-deck noise sheet: the FIRST draw in the scene pass, depth Always and
 * no write, so the terrain draws over it. Sun and moon are their own additive draws right after. */
#ifndef SKYSTAGE_H
#define SKYSTAGE_H

#include "DrawStage.h"

namespace outshine::Render {

class SkyStage : public DrawStage {
public:
  /* Borrowed atmosphere resources; this holds only the pipeline/bind group built from them. */
  void Configure(const Gpu &gpu, wgpu::TextureView skyLutView, wgpu::Sampler lutSamp, wgpu::Buffer atmoBuf);
  void Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
};

} // namespace outshine::Render
#endif
