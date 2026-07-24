/* FlightBox — FBCloudMipDownStage: the box-downsample compute (kMipDownCS) shared by every 3D
 * noise-volume bake (FBCloudBaseBakeStage, FBCloudDetailBakeStage) to build a full mip chain after
 * the base-level noise compute. Not a per-frame draw stage — a one-time INIT-time bake helper, so it
 * owns and submits its own command buffers rather than recording into a caller's pass/encoder. */
#ifndef FBCLOUDMIPDOWNSTAGE_H
#define FBCLOUDMIPDOWNSTAGE_H

#include <webgpu/webgpu_cpp.h>
#include "FBGpu.h"

namespace FlightBox {

class FBCloudMipDownStage {
public:
  void Configure(const FBGpu &gpu);

  /* Box-average `srcView` (a single mip level of a 3D texture, size 2*dstSize per axis) 2x2x2 into
   * `dstView` (a storage-binding 3D texture view of dstSize^3) — submits its own command buffer. */
  void Downsample(wgpu::TextureView srcView, wgpu::TextureView dstView, uint32_t dstSize);

private:
  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::ComputePipeline Pipe;
};

} // namespace FlightBox
#endif
