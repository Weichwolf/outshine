/* THE ONE SCALE. Turns the two sky LUTs into the two irradiances every lit surface needs, in the
 * same units the sky-view LUT is in (top-of-atmosphere solar irradiance = 1). Sky and ground are on
 * one scene-referred scale exactly because the ground's ambient IS the integral of the sky the sky
 * pass draws — not a second constant fitted against it.
 *
 * Compute-only, one workgroup, once per frame. It rides in the sky-view compute pass, after the
 * dispatch that writes the LUT it reads: the per-frame render pass count is unchanged. */
#ifndef IRRADIANCESTAGE_H
#define IRRADIANCESTAGE_H

#include "DrawStage.h"

namespace outshine::Render {

class IrradianceStage : public DrawStage {
public:
  void Configure(const Gpu &gpu, wgpu::Buffer irrBuf, wgpu::TextureView skyLutView,
                 wgpu::TextureView transLutView, wgpu::Sampler lutSamp, wgpu::Buffer atmoBuf);
  void EncodeCompute(const FrameContext &ctx, wgpu::ComputePassEncoder &pass) override;

  /* sunIrr : vec4f (direct normal, w spare) + skyIrr : vec4f (diffuse on horizontal, w = the
   * luminance of E on a horizontal surface — sun and sky together, ExposureStage's only input) */
  static constexpr uint64_t kBufferBytes = 2 * 4 * sizeof(float);
  static constexpr int kFloats = 2 * 4;

private:
  wgpu::ComputePipeline Pipe;
  wgpu::BindGroup Bind;
};

} // namespace outshine::Render
#endif
