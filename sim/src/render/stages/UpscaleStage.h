/* Samples the fixed-720p FrameTex onto the target at display resolution. Bilinear; TODO
 * bicubic/sharpen. Used by BOTH present paths — it does not care what filled FrameTex. */
#ifndef UPSCALESTAGE_H
#define UPSCALESTAGE_H

#include "DrawStage.h"

namespace outshine::Render {

class UpscaleStage : public DrawStage {
public:
  /* Named Configure and not Init, so it does not hide DrawStage's virtual Init(gpu)
   * (-Woverloaded-virtual). */
  void Configure(const Gpu &gpu, wgpu::TextureView frameView);
  void Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
  wgpu::Sampler Samp;
};

} // namespace outshine::Render
#endif
