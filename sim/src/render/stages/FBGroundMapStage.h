/* The radar ground map's raster, drawn UNDER the FCR page's symbology inside the HUD pass FBRenderer
 * already opened — a stage, so the per-frame Begin*Pass count is untouched. It draws the bay's own
 * rectangle: black where the sector is not, the backscatter texture where it is.
 * Vertrag: doc/render/renderer.md, Abschnitt 2. */
#ifndef FBGROUNDMAPSTAGE_H
#define FBGROUNDMAPSTAGE_H

#include "FBDrawStage.h"
#include "FBDisplaySystem.h"
#include "FBState.h"

namespace FlightBox::Render {

class FBGroundMapStage : public FBDrawStage {
public:
  void Init(const FBGpu &gpu) override;

  /* The bay to fill, in frame pixels, and the block that fills it. `bay` degenerate = nothing to draw,
   * which is what the stage self-gates on. */
  void SetTarget(const Systems::FBMfdBayRect &bay, const FBGroundMapBlock &map, bool have);

  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
  wgpu::Buffer Uni;
  wgpu::Texture Raster;
  wgpu::Sampler Samp;
  Systems::FBMfdBayRect Bay{0, 0, 0, 0};
  FBGroundMapBlock Map{};
  bool Have = false;
};

} // namespace FlightBox::Render
#endif
