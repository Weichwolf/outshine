/* THE TEMPORAL RESOLVE: this frame's jittered radiance blended into the accumulated history at the
 * place the same world point occupied last frame.
 *
 * It is the OTHER half of TemporalJitter and useless without it. The jitter puts the sample point
 * somewhere else inside the pixel each frame; this stage is what turns that sequence into one image
 * whose coverage is the average — which is the only mechanism in the tree that can draw a sub-pixel
 * blade at all, because no post filter can invent coverage that was never rasterised.
 *
 * It owns TWO history textures and ping-pongs them: a bind group pins a texture view at creation, so
 * "read the other one" is two bind groups and not a rebind. Renderer asks for the frame's output view
 * BEFORE it opens the pass, exactly as it does for the cloud resolve. */
#ifndef TAASTAGE_H
#define TAASTAGE_H

#include "DrawStage.h"

namespace outshine::Render {

class TaaStage : public DrawStage {
public:
  /* `hdrView` is the scene target this frame was drawn into, `velView` the motion attachment beside
   * it, `depthView` the reversed-Z depth the static half of the motion is reconstructed from, and
   * `atmoBuf` the camera basis that reconstruction needs (the SAME camRay() the sky uses). */
  void Configure(const Gpu &gpu, wgpu::Sampler samp, wgpu::TextureView hdrView,
                 wgpu::TextureView velView, wgpu::TextureView depthView, wgpu::Buffer atmoBuf,
                 int width, int height);

  void Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

  /* The attachment Renderer opens the resolve pass on, and the texture the tonemap then reads. */
  wgpu::TextureView Output(unsigned frameNo) const { return Hist[frameNo & 1u].CreateView(); }
  wgpu::TextureView History(int i) const { return Hist[i & 1].CreateView(); }
  long HistoryBytes(void) const { return Bytes; }

private:
  static constexpr int kUniFloats = 28;   /* mat4 + eyeDelta + jitter + par — the WGSL `T` verbatim */

  wgpu::Queue Queue;
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind[2];
  wgpu::Buffer Uni;
  wgpu::Texture Hist[2];
  long Bytes = 0;
  int Width = 0, Height = 0;
};

} // namespace outshine::Render
#endif
