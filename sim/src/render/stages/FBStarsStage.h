/* The HYG star field: additive instanced quads at true alt/az, over the sky and under the terrain.
 * Self-gates on SVS, daylight and a missing catalogue. */
#ifndef FBSTARSSTAGE_H
#define FBSTARSSTAGE_H

#include <vector>
#include "FBDrawStage.h"

namespace FlightBox::Render {

class FBStarsStage : public FBDrawStage {
public:
  void Init(const FBGpu &gpu) override;

  /* 6 B/star, mag-sorted. Call before Update(); a miss leaves no stars. */
  void SetCatalogue(const uint8_t *hyg, int nbytes, double originLat, double originLon);
  /* At most every 20 s: sidereal drift stays sub-pixel between rebuilds. Positions are at
   * "infinity" and therefore eye-independent, so this needs no eye. */
  void Update(double nowSec);

  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::RenderPipeline Pipe;
  wgpu::Buffer Inst, Uni;
  wgpu::BindGroup Bind;
  std::vector<float> Cat;         /* catalogue: ra,dec,mag,bv per star (4 floats) */
  std::vector<float> Dir;         /* cached visible: e,u,n,bright,r,g,b (7 floats) */
  double Lat = 0, Lon = 0, DirAt = -1e30;
  int NStars = 0, NStarVis = 0, InstCap = 0;
};

} // namespace FlightBox::Render
#endif
