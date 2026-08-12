/* Samples the declared-size FrameTex over the present viewport, which is usually SMALLER than it.
 * Bilinear; TODO bicubic/sharpen. Used by BOTH present paths — it does not care what filled
 * FrameTex. */
#ifndef PRESENTSTAGE_H
#define PRESENTSTAGE_H

#include "DrawStage.h"

namespace outshine::Render {

class PresentStage : public DrawStage {
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
