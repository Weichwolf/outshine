/* THE ONE SEAM an equipped body's avionics reaches the frame through. It is an interface and not a
 * member because a HUD is a vehicle capability, not renderer equipment: a pedestrian registers none
 * and the whole avionics layer is then absent from the link, not merely switched off.
 * Renderer still opens and closes the overlay pass exactly as it opens every other one — an
 * implementation only records into the borrowed encoder. */
#ifndef OVERLAYSTAGE_H
#define OVERLAYSTAGE_H

#include <webgpu/webgpu_cpp.h>
#include "FrameContext.h"
#include "Gpu.h"

namespace outshine::Render {

class OverlayStage {
public:
  virtual ~OverlayStage() = default;

  /* `sceneHdr` is the pre-tonemap scene target: an image intensifier amplifies the radiance the scene
   * pass computed, so the overlay is handed the same view TonemapStage reads. */
  virtual void Init(const Gpu &gpu, const wgpu::Sampler &samp, const wgpu::TextureView &sceneHdr) = 0;

  /* False = Renderer opens no overlay pass at all; the pass count is the invariant, so the decision
   * sits outside the pass and an empty overlay costs nothing. */
  virtual bool Active(void) const = 0;

  /* The out-the-window viewport's lower edge inside a frame of `frameH`. A cockpit claims the bottom
   * row of its 3x3 grid for the MFD bank; anything else claims nothing and the scene keeps the frame. */
  virtual int SceneViewH(int frameH) const { return frameH; }

  /* Per-frame CPU work — bay layout, symbology — before any pass is opened. */
  virtual void Update(const FrameContext &ctx) { (void)ctx; }

  virtual void Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) = 0;
};

} // namespace outshine::Render
#endif
