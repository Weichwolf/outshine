/* The night-light field: instanced additive sprites, streamed and placed by World, drawn after the
 * terrain and depth-tested for occlusion. Self-gates like StarsStage. */
#ifndef TILELIGHTSSTAGE_H
#define TILELIGHTSSTAGE_H

#include "DrawStage.h"

namespace outshine::Render {

class TileLightsStage : public DrawStage {
public:
  void Init(const Gpu &gpu) override;

  /* Set once: the ECEF the instance positions are relative to. */
  void SetAnchor(const double anchor[3]) { for (int i = 0; i < 3; i++) Anchor[i] = anchor[i]; }
  /* count * 7 floats [posRelAnchor.xyz, worldRadiusM, colorPremul.rgb]. */
  void SetLights(const float *inst, int count);

  void Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::RenderPipeline Pipe;
  wgpu::Buffer Inst, Uni;
  wgpu::BindGroup Bind;
  double Anchor[3] = {0, 0, 0};
  int NLights = 0, InstCap = 0;
};

} // namespace outshine::Render
#endif
