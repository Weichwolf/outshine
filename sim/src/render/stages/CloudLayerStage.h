/* The cloud chain, ALL OF IT: one stage, one shader, one full-resolution pass, no bakes, no history.
 * It marches the three weather decks as spherical shells (ray ∩ shell, analytic) and blends the result
 * straight into the HDR scene target, which is why the tonemap has nothing to do with clouds any more.
 * The density it marches is core/CloudDensity.h — the same function a sensor will evaluate.
 * Spec + state: doc/render/clouds.md. */
#ifndef CLOUDLAYERSTAGE_H
#define CLOUDLAYERSTAGE_H

#include "DrawStage.h"
#include "CloudDensity.h"

namespace outshine::Render {

class CloudLayerStage : public DrawStage {
public:
  /* `cloudBuf` is Renderer's ONE cloud field (stages/CloudShadow.h) — this stage marches exactly what
   * the lit surfaces shadow themselves against, and writes none of it itself. */
  void Configure(const Gpu &gpu, wgpu::Buffer atmoBuf, wgpu::Buffer cloudBuf, wgpu::Sampler lutSamp,
                 wgpu::TextureView skyLUTView, wgpu::TextureView transLUTView,
                 wgpu::TextureView depthView);

  /* The weather sample for this frame, from the client via Renderer::SetCloudSky. */
  void SetSky(const CloudSky &sky) { Sky = sky; }
  /* Scales the march's step counts. 0 — the DEFAULT — draws the deck as a sheet instead, which costs
   * no pass of its own and no march (doc/render/clouds.md). */
  void SetQuality(double q) { Quality = q; }

  /* Renderer asks BEFORE opening a pass: no march, no pass. The pass count is therefore a function of
   * the weather AND the quality, both of which are logged with it (render/passcount). */
  bool Active(void) const { return Quality > 0.0 && Sky.Any(); }
  bool SheetActive(void) const { return Quality <= 0.0 && Sky.Any(); }

  /* Per frame, before Encode. The FIELD is Renderer's upload (Renderer::WriteCloudSky) — this is only
   * the deck geometry's log line. */
  void Update(const FrameContext &ctx);

  void Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) override;
  /* The sheet rides in the SCENE pass, over the sky and under the terrain — no pass of its own. */
  void EncodeSheet(const FrameContext &ctx, wgpu::RenderPassEncoder &pass);

private:
  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
  wgpu::RenderPipeline SheetPipe;
  wgpu::BindGroup SheetBind;

  CloudSky Sky;
  double Quality = 0.0;

  bool LoggedOnce = false;
  float LoggedBase[3] = {-1.0f, -1.0f, -1.0f};
};

} // namespace outshine::Render
#endif
