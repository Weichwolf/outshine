/* The moon as a lit sphere, an ADDITIVE draw right after SkyStage. It OWNS the NASA LROC albedo
 * texture, being its sole consumer. */
#ifndef MOONSTAGE_H
#define MOONSTAGE_H

#include "DrawStage.h"
#include <cstdint>

namespace outshine::Render {

class MoonStage : public DrawStage {
public:
  /* Missing or short bytes fall back to a 1x1 mid-grey. */
  void Configure(const Gpu &gpu, wgpu::Buffer atmoBuf, wgpu::Sampler lutSamp,
                 const uint8_t *rgba, size_t rgbaBytes, int w, int h);
  void Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  wgpu::Texture Tex;   /* equirect RGBA8Srgb; 1x1 grey fallback if unset */
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
};

} // namespace outshine::Render
#endif
