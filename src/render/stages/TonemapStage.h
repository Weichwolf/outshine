/* THE DISPLAY TRANSFER ON ITS OWN: one full-screen fragment from the resolved linear radiance to the
 * frame the picture is read off. It is MACHINERY -- without it a plan that asks for a picture has no
 * path from radiance to a frame at all -- so the compiler pulls it and no declaration names it.
 *
 * IT IS THE UNFUSED HALF OF A PAIR. Where a plan also declares the temporal resolve, R2 fuses the two
 * into one fragment writing two attachments and TaaStage is that implementation; where it does not,
 * this stage reads the scene target the alias bound in the resolve's place and this is the whole
 * cost of the picture. Both emit the same display transfer, from stages/Resolve.h, so there is one
 * statement of what a display frame is. */
#ifndef TONEMAPSTAGE_H
#define TONEMAPSTAGE_H

#include "DrawStage.h"
#include "Resolve.h"

namespace outshine::Render {

class TonemapStage : public DrawStage {
public:
  /* `aoView` and `meterBuf` are bound only where `options` says they exist -- a plan without them
   * generates a shader that does not name them, so there is no neutral texture to stand in. */
  void Configure(const Gpu &gpu, wgpu::TextureView linearView, wgpu::TextureView aoView,
                 wgpu::Buffer meterBuf, const DisplayOptions &options);
  void Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
};

} // namespace outshine::Render
#endif
