/* The device-level handles a stage gets, handed once at Init and never per frame. Renderer owns
 * the real device/queue/swapchain; a stage never re-derives or requests them itself. */
#ifndef GPU_H
#define GPU_H

#include <webgpu/webgpu_cpp.h>

namespace outshine::Render {

struct Gpu {
  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::TextureFormat HdrFormat;       /* offscreen HDR scene target format (stages that draw into it) */
  wgpu::TextureFormat SurfaceFormat;   /* swapchain/present format (stages that draw into FrameTex/final) */
  int Width, Height;                  /* fixed scene resolution (FrameTex), not the live swapchain size */
  /* Whether the device granted `float32-filterable`. A stage that wants an EXACT texel and a filter
   * over exact texels needs both, and the two are one feature: without it a 32-bit float texture may
   * only be sampled unfiltered. */
  bool FiltersFloat32 = false;
};

/* What a SUN-LIT surface binds, as one bundle: IrradianceStage's two irradiances and
 * cascade uniform plus its atlas. Terrain and buildings take the same four handles, which is
 * the C++ half of the promise stages/SurfaceLight.h makes in WGSL — one light, one scale, one set of
 * cascades, so no surface can end up lit by a second sun. */
struct SceneLight {
  wgpu::Buffer Irradiance;
  wgpu::Buffer Cascades;
  wgpu::TextureView ShadowAtlas;
  wgpu::Sampler ShadowCompare;
  /* The three decks plus the anchor frame their horizontal field is measured in — the SAME buffer
   * CloudLayerStage marches. A second cloud field would put the shadow somewhere other than under
   * the cloud, so there is one and every lit surface reads it. */
};

} // namespace outshine::Render
#endif
