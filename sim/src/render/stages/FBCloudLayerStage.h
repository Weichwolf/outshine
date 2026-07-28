/* The cloud chain, ALL OF IT: one stage, one shader, one full-resolution pass, no bakes, no history.
 * It marches the three weather decks as spherical shells (ray ∩ shell, analytic) and blends the result
 * straight into the HDR scene target, which is why the tonemap has nothing to do with clouds any more.
 * The density it marches is core/FBCloudDensity.h — the same function a sensor will evaluate.
 * Spec + state: doc/render/clouds.md. */
#ifndef FBCLOUDLAYERSTAGE_H
#define FBCLOUDLAYERSTAGE_H

#include "FBDrawStage.h"
#include "FBCloudDensity.h"

namespace FlightBox::Render {

class FBCloudLayerStage : public FBDrawStage {
public:
  void Configure(const FBGpu &gpu, wgpu::Buffer atmoBuf, wgpu::Sampler lutSamp,
                 wgpu::TextureView skyLUTView, wgpu::TextureView transLUTView,
                 wgpu::TextureView depthView);

  /* The weather sample for this frame, from the client via FBRenderer::SetCloudSky. */
  void SetSky(const FBCloudSky &sky) { Sky = sky; }
  void SetQuality(double q) { Quality = q; }   /* scales the step counts; 0 disables the pass entirely */

  /* FBRenderer asks BEFORE opening a pass: no deck, no pass. The pass count is therefore a function of
   * the weather, which is logged with it (render/passcount). */
  bool Active(void) const { return Quality > 0.0 && Sky.Any(); }

  /* Per frame, before Encode: uploads the decks + the anchor frame the shader measures position in. */
  void Update(const FBFrameContext &ctx);

  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
  wgpu::Buffer Uni;

  FBCloudSky Sky;
  double Quality = 1.0;

  /* The tangent frame the horizontal cloud coordinate is measured in. Pinned at the first frame so the
   * field stands still in the world; the camera's offset from it is re-measured in double each frame
   * (the difference of two ECEF positions is the one place f32 would actually lose metres). */
  bool LoggedOnce = false;
  float LoggedBase[3] = {-1.0f, -1.0f, -1.0f};

  bool HaveAnchor = false;
  double Anchor[3] = {0, 0, 0};
  double AxisE[3] = {1, 0, 0}, AxisN[3] = {0, 1, 0};
};

} // namespace FlightBox::Render
#endif
