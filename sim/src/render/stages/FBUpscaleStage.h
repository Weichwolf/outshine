/* FlightBox — FBUpscaleStage: samples the fixed-720p FrameTex (scene+tonemap+HUD all land there) onto
 * the swapchain/offscreen target at display resolution. Bilinear today; TODO bicubic/sharpen. Reused
 * at TWO call sites (the normal present pass and the boot-loading-screen present pass) — both just
 * hand it a RenderPassEncoder, the stage doesn't care what filled FrameTex. */
#ifndef FBUPSCALESTAGE_H
#define FBUPSCALESTAGE_H

#include "FBDrawStage.h"

namespace FlightBox {

class FBUpscaleStage : public FBDrawStage {
public:
  /* `frameView` is the fixed-720p present source (FBRenderer's FrameTex) — set once, sampled every
   * Encode() regardless of what most recently rendered into it. Named Configure, not Init, so it
   * doesn't hide FBDrawStage's virtual Init(gpu) (-Woverloaded-virtual). */
  void Configure(const FBGpu &gpu, wgpu::TextureView frameView);
  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
  wgpu::Sampler Samp;
};

} // namespace FlightBox
#endif
