/* FlightBox — FBTonemapStage: one shader source, TWO pipelines (kTonemapWGSL with cloud compositing,
 * kTonemapPlainWGSL without) — same ACES compress either way, chosen at Encode() by whether the cloud
 * path is armed (a boot-time constant, never toggled mid-run). When armed, it borrows the cloud-resolve
 * peer to read WHICH ping-pong history slot this frame resolved into (never owns it). */
#ifndef FBTONEMAPSTAGE_H
#define FBTONEMAPSTAGE_H

#include "FBDrawStage.h"
#include "FBCloudResolveStage.h"

namespace FlightBox {

class FBTonemapStage : public FBDrawStage {
public:
  /* `cloudResolve` is nullptr when the cloud path isn't armed (Plain-only). */
  void Configure(const FBGpu &gpu, wgpu::Sampler samp, wgpu::TextureView hdrView,
                 bool cloudsOn, const FBCloudResolveStage *cloudResolve);
  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  bool CloudsOn = false;
  const FBCloudResolveStage *CloudResolve = nullptr;

  wgpu::RenderPipeline TonemapPipe, TonemapPlainPipe;
  wgpu::BindGroup TonemapBindH[2], TonemapBindPlain;
};

} // namespace FlightBox
#endif
