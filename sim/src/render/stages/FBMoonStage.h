/* The moon as a lit sphere, an ADDITIVE draw right after FBSkyStage. It OWNS the NASA LROC albedo
 * texture, being its sole consumer. */
#ifndef FBMOONSTAGE_H
#define FBMOONSTAGE_H

#include "FBDrawStage.h"
#include <cstdint>

namespace FlightBox {

class FBMoonStage : public FBDrawStage {
public:
  /* Missing or short bytes fall back to a 1x1 mid-grey. */
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
