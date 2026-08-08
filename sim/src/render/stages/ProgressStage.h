/* A BAR FROM A FRACTION, and that is all it knows. It is not an overlay's — an overlay is a vehicle
 * capability and a pedestrian registers none, so a bar hanging off it would be invisible in exactly
 * the client that needs it. It is not a phase either: WHO is loading and WHEN that ends belongs to
 * the application, and this stage would draw a fuel gauge with the same code.
 * Texture-free — the bar is geometry over a uniform. */
#ifndef PROGRESSSTAGE_H
#define PROGRESSSTAGE_H

#include "DrawStage.h"

namespace outshine::Render {

class ProgressStage : public DrawStage {
public:
  void Init(const Gpu &gpu) override;
  /* Unrounded and unsmoothed: what the bar shows is the number it was handed, so a stalled source
   * is a bar that stands still rather than one that keeps creeping. */
  void SetFraction(const wgpu::Queue &queue, float fraction);
  void Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
  wgpu::Buffer Uni;
};

} // namespace outshine::Render
#endif
