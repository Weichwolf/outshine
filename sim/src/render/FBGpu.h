/* The device-level handles a stage gets, handed once at Init and never per frame. FBRenderer owns
 * the real device/queue/swapchain; a stage never re-derives or requests them itself. */
#ifndef FBGPU_H
#define FBGPU_H

#include <webgpu/webgpu_cpp.h>

namespace FlightBox {

struct FBGpu {
  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::TextureFormat HdrFormat;       /* offscreen HDR scene target format (stages that draw into it) */
  wgpu::TextureFormat SurfaceFormat;   /* swapchain/present format (stages that draw into FrameTex/final) */
  int Width, Height;                  /* fixed scene resolution (FrameTex), not the live swapchain size */
  wgpu::Instance Instance;             /* only for the rare stage that blocks on MapAsync (Instance::WaitAny) */
};

} // namespace FlightBox
#endif
