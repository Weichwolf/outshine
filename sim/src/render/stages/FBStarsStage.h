/* FlightBox — FBStarsStage: the HYG star field (EVS night). Additive instanced quads at true alt/az,
 * drawn in the scene pass over the sky, under the terrain. Self-gates: no draw in SVS, daylight, or
 * before a catalogue is loaded. */
#ifndef FBSTARSSTAGE_H
#define FBSTARSSTAGE_H

#include <vector>
#include "FBDrawStage.h"

namespace FlightBox {

class FBStarsStage : public FBDrawStage {
public:
  void Init(const FBGpu &gpu) override;

  /* HYG catalogue bytes (6 B/star, mag-sorted). Call before Update(); a miss leaves no stars. */
  void SetCatalogue(const uint8_t *hyg, int nbytes, double originLat, double originLon);
  /* Rebuild the visible-star instance buffer at most every 20 s (sidereal drift stays sub-pixel
   * between rebuilds). Positions are camera-relative ECEF at "infinity" — eye-independent, so this
   * does not need the eye; the shared camera-relative MVP in Encode() supplies the per-frame rotation. */
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

} // namespace FlightBox
#endif
