/* The box-downsample every 3D noise bake shares. An INIT-time helper, not a per-frame stage — which
 * is why it may own and submit its own command buffers instead of borrowing an encoder. */
#ifndef FBCLOUDMIPDOWNSTAGE_H
#define FBCLOUDMIPDOWNSTAGE_H

#include <webgpu/webgpu_cpp.h>
#include "FBGpu.h"

namespace FlightBox {

class FBCloudMipDownStage {
public:
  void Configure(const FBGpu &gpu);

  /* 2x2x2 box average; `srcView` must be 2*dstSize per axis. */
  void Downsample(wgpu::TextureView srcView, wgpu::TextureView dstView, uint32_t dstSize);

private:
  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::ComputePipeline Pipe;
};

} // namespace FlightBox
#endif
