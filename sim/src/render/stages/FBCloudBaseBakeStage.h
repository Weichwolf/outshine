/* The 128^3 Perlin-Worley base-shape volume, baked ONCE at Configure() — not a per-frame stage. Owns
 * ShapeStats because that lab tool reads this exact texture. */
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

  /* Evaluates the SAME math the march's density() uses, so tuning is numbers-driven. */
  void ShapeStats(float cover, float low, float high);

private:
  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::Instance Instance;   /* ShapeStats blocks on MapAsync via Instance::WaitAny (native path) */
  wgpu::Texture Tex;   /* 128^3 RGBA8Unorm, full mip chain */
};

} // namespace FlightBox
#endif
