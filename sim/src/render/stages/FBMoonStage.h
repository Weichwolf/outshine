/* FlightBox — FBMoonStage: the moon-as-lit-sphere reconstruction, split out of FBSkyStage's kSkyWGSL
 * into its own ADDITIVE draw (One/One blend) — same scene pass, encoded directly after FBSkyStage.
 * Owns the NASA LROC albedo texture (the sole consumer after the split — MoonTex moves here from
 * FBRenderer, per the "single consumer owns the resource" rule already used for e.g. TerrainPipe). */
#ifndef FBMOONSTAGE_H
#define FBMOONSTAGE_H

#include "FBDrawStage.h"
#include <cstdint>

namespace FlightBox {

class FBMoonStage : public FBDrawStage {
public:
  /* `rgba`/`rgbaBytes`/`w`/`h` = the raw NASA LROC equirect bytes FBRenderer::SetMoonTexture staged
   * (w/h <= 0 or rgbaBytes too small for w*h*4 -> a 1x1 mid-grey fallback, same as the original). */
  void Configure(const FBGpu &gpu, wgpu::Buffer atmoBuf, wgpu::Sampler lutSamp,
                 const uint8_t *rgba, size_t rgbaBytes, int w, int h);
  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  wgpu::Texture Tex;   /* equirect RGBA8Srgb; 1x1 grey fallback if unset */
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
};

} // namespace FlightBox
#endif
