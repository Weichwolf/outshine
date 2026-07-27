/* The 512^2 F1 cell field, baked ONCE. A 2D texture with no mip chain, unlike the 3D bakes; the
 * march samples it through a horizontal tangent-plane projection. */
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
