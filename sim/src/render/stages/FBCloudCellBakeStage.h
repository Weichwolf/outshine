/* FlightBox — FBCloudCellBakeStage: the 512² 2D F1-round cell field (B-mode vertical puffs), baked ONCE
 * at Configure() — a 2D texture, no mip chain (unlike the 3D base/detail bakes). FBCloudMarchStage
 * samples it via a horizontal tangent-plane projection (view injected at ITS Configure()). */
#ifndef FBCLOUDCELLBAKESTAGE_H
#define FBCLOUDCELLBAKESTAGE_H

#include <webgpu/webgpu_cpp.h>
#include "FBGpu.h"

namespace FlightBox {

class FBCloudCellBakeStage {
public:
  void Configure(const FBGpu &gpu);
  wgpu::TextureView GetView(void) const { return Tex.CreateView(); }

private:
  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::Texture Tex;   /* 512^2 RGBA8Unorm, single mip */
};

} // namespace FlightBox
#endif
