/* The night-light field: instanced additive sprites, streamed and placed by FBWorld, drawn after the
 * terrain and depth-tested for occlusion. Self-gates like FBStarsStage. */
#ifndef FBTILELIGHTSSTAGE_H
#define FBTILELIGHTSSTAGE_H

#include "FBDrawStage.h"

namespace FlightBox {

class FBTileLightsStage : public FBDrawStage {
public:
  void Init(const FBGpu &gpu) override;

  /* Set once: the ECEF the instance positions are relative to. */
  void SetAnchor(const double anchor[3]) { for (int i = 0; i < 3; i++) Anchor[i] = anchor[i]; }
  /* count * 7 floats [posRelAnchor.xyz, worldRadiusM, colorPremul.rgb]. */
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
