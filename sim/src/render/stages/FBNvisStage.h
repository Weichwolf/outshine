/* NIGHT VISION on the IRST page: an IMAGE INTENSIFIER, and deliberately that and not a thermal
 * imager. It amplifies the very radiance the scene pass already computed (HdrTex, linear, pre-tonemap
 * — photons, which is exactly what a tube multiplies), crops it gnomonically about the boresight,
 * squeezes it through the tube's transfer and paints it on P43 green.
 *
 * WHAT THIS APPROXIMATION DOES NOT MODEL, because the scene does not carry it: the near-infrared band
 * (a tube is most sensitive around 800 nm, where foliage is far brighter than the eye sees it), any
 * EMITTED heat at all (a warm exhaust is invisible here — this is not a FLIR), blooming and halo
 * around point sources, tube scintillation noise, and the automatic gain that a real tube runs against
 * the scene mean. What it DOES get right is the geometry, the scene's own illumination — moon phase,
 * moon and sun elevation and the night-lights pass all reach it through HdrTex — and the fact that an
 * intensified picture is monochrome. */
#ifndef FBNVISSTAGE_H
#define FBNVISSTAGE_H

#include "FBDrawStage.h"
#include "FBDisplaySystem.h"

namespace FlightBox::Render {

class FBNvisStage : public FBDrawStage {
public:
  /* The HDR scene view is handed in after the offscreen targets exist, like FBTonemapStage's. */
  void Configure(const FBGpu &gpu, wgpu::Sampler samp, wgpu::TextureView hdrView);

  /* The bay to fill, in frame pixels; a degenerate rect is the stage's self-gate. */
  void SetTarget(const Systems::FBMfdBayRect &bay, bool have) {
    Bay = bay;
    Have = have && bay.X1 - bay.X0 > 2.f && bay.Y1 - bay.Y0 > 2.f;
  }

  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  wgpu::Queue Queue;
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
  wgpu::Buffer Uni;
  Systems::FBMfdBayRect Bay{0, 0, 0, 0};
  bool Have = false;
};

} // namespace FlightBox::Render
#endif
