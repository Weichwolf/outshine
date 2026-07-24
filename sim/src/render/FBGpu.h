/* FlightBox — FBGpu: the device-level WebGPU handles every draw stage needs, handed once at Init
 * (never per-frame). FBRenderer owns the actual device/queue/swapchain; this is the read-only view a
 * stage gets of them — a stage never re-derives or requests these itself. */
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
};

} // namespace FlightBox
#endif
