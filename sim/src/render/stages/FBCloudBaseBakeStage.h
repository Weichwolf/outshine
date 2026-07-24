/* FlightBox — FBCloudBaseBakeStage: the Perlin-Worley 3D base-shape noise volume (128^3), baked ONCE
 * at Configure() via FBCloudMipDownStage for the mip chain — not a per-frame draw stage. FBCloudMarchStage
 * samples the result (view injected at ITS Configure()). Also owns ShapeStats (the numeric shape/
 * density histogram lab tool, FB_SHAPEHIST) since it reads this exact texture. */
#ifndef FBCLOUDBASEBAKESTAGE_H
#define FBCLOUDBASEBAKESTAGE_H

#include <webgpu/webgpu_cpp.h>
#include "FBGpu.h"
#include "FBCloudMipDownStage.h"

namespace FlightBox {

class FBCloudBaseBakeStage {
public:
  void Configure(const FBGpu &gpu, FBCloudMipDownStage &mipDown);
  wgpu::TextureView GetView(void) const {
    wgpu::TextureViewDescriptor vd{};
    vd.dimension = wgpu::TextureViewDimension::e3D;
    return Tex.CreateView(&vd);
  }

  /* Numeric shape/density histogram over a 3D grid (FB_SHAPEHIST lab tool): evaluate the SAME
   * base-shape math the march's density() uses and read back percentiles, so tuning is numbers-
   * driven, not eyeballed. */
  void ShapeStats(float cover, float low, float high);

private:
  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::Instance Instance;   /* ShapeStats blocks on MapAsync via Instance::WaitAny (native path) */
  wgpu::Texture Tex;   /* 128^3 RGBA8Unorm, full mip chain */
};

} // namespace FlightBox
#endif
