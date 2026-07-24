/* FlightBox — FBTileLightsStage: night-light field (EVS night). Instanced additive sprites at ground
 * level, camera-anchor-relative ECEF, class-coloured. Streamed + placed by FBWorld, drawn in the
 * scene pass after terrain (depth-tested for occlusion). Self-gates: no draw in SVS, daylight, or
 * before FBWorld has streamed any. */
#ifndef FBTILELIGHTSSTAGE_H
#define FBTILELIGHTSSTAGE_H

#include "FBDrawStage.h"

namespace FlightBox {

class FBTileLightsStage : public FBDrawStage {
public:
  void Init(const FBGpu &gpu) override;

  /* ECEF anchor the streamed instance positions are relative to (the world origin) — set once. */
  void SetAnchor(const double anchor[3]) { for (int i = 0; i < 3; i++) Anchor[i] = anchor[i]; }
  /* `inst` = count * 7 floats [posRelAnchor.xyz, worldRadiusM, colorPremul.rgb]. */
  void SetLights(const float *inst, int count);

  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::RenderPipeline Pipe;
  wgpu::Buffer Inst, Uni;
  wgpu::BindGroup Bind;
  double Anchor[3] = {0, 0, 0};
  int NLights = 0, InstCap = 0;
};

} // namespace FlightBox
#endif
