/* ONE shader source, TWO pipelines: same ACES compress, with and without the cloud composite. Which
 * one applies is a BOOT-TIME constant, never toggled mid-run. */
#ifndef FBTONEMAPSTAGE_H
#define FBTONEMAPSTAGE_H

#include "FBDrawStage.h"
#include "FBCloudResolveStage.h"

namespace FlightBox::Render {

class FBTonemapStage : public FBDrawStage {
public:
  /* nullptr when the cloud path is not armed. */
  void Configure(const FBGpu &gpu, wgpu::Sampler samp, wgpu::TextureView hdrView,
                 bool cloudsOn, const FBCloudResolveStage *cloudResolve);
  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  bool CloudsOn = false;
  const FBCloudResolveStage *CloudResolve = nullptr;

  wgpu::RenderPipeline TonemapPipe, TonemapPlainPipe;
  wgpu::BindGroup TonemapBindH[2], TonemapBindPlain;
};

} // namespace FlightBox::Render
#endif
