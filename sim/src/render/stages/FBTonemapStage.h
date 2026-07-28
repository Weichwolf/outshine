/* ONE shader, ONE pipeline: the ACES compress from the HDR scene target to the frame target. It used
 * to carry a second variant that composited a separate cloud texture; since the cloud pass blends into
 * HdrTex itself (render/stages/FBCloudLayerStage), there is nothing left to switch on. */
#ifndef FBTONEMAPSTAGE_H
#define FBTONEMAPSTAGE_H

#include "FBDrawStage.h"

namespace FlightBox {

class FBTonemapStage : public FBDrawStage {
public:
  void Configure(const FBGpu &gpu, wgpu::Sampler samp, wgpu::TextureView hdrView);
  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  wgpu::RenderPipeline TonemapPipe;
  wgpu::BindGroup TonemapBind;
};

} // namespace FlightBox
#endif
