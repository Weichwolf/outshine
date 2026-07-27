/* Samples the fixed-720p FrameTex onto the target at display resolution. Bilinear; TODO
 * bicubic/sharpen. Used by BOTH present paths — it does not care what filled FrameTex. */
#ifndef FBUPSCALESTAGE_H
#define FBUPSCALESTAGE_H

#include "FBDrawStage.h"

namespace FlightBox {

class FBUpscaleStage : public FBDrawStage {
public:
  /* Named Configure and not Init, so it does not hide FBDrawStage's virtual Init(gpu)
   * (-Woverloaded-virtual). */
  void Configure(const FBGpu &gpu, wgpu::TextureView frameView);
  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
  wgpu::Sampler Samp;
};

} // namespace FlightBox
#endif
