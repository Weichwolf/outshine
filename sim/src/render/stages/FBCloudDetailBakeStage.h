/* FlightBox — FBCloudDetailBakeStage: the Worley-octave 3D detail noise volume (32^3), baked ONCE at
 * Configure() via FBCloudMipDownStage for the mip chain — not a per-frame draw stage. FBCloudMarchStage
 * erodes the base shape with this (view injected at ITS Configure()). */
#ifndef FBCLOUDDETAILBAKESTAGE_H
#define FBCLOUDDETAILBAKESTAGE_H

#include <webgpu/webgpu_cpp.h>
#include "FBGpu.h"
#include "FBCloudMipDownStage.h"

namespace FlightBox {

class FBCloudDetailBakeStage {
public:
  void Configure(const FBGpu &gpu, FBCloudMipDownStage &mipDown);
  wgpu::TextureView GetView(void) const {
    wgpu::TextureViewDescriptor vd{};
    vd.dimension = wgpu::TextureViewDimension::e3D;
    return Tex.CreateView(&vd);
  }

private:
  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::Texture Tex;   /* 32^3 RGBA8Unorm, full mip chain */
};

} // namespace FlightBox
#endif
