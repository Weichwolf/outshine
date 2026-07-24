/* FlightBox — FBDrawStage: one shader's pipeline(s) + bind group(s) + draw(s), recorded into a
 * BORROWED encoder FBRenderer already opened. A stage NEVER begins or ends a pass itself — pass
 * topology (Begin/EndRenderPass boundaries, the encode order) stays FBRenderer's, exactly as today;
 * splitting the renderer into stages must not add a single render or compute pass. A stage self-gates
 * its own draw (e.g. "nothing visible this frame" -> Encode records no commands) rather than the
 * caller deciding whether to invoke it — FBRenderer calls every stage in its slot unconditionally.
 * Override whichever Encode() form matches the stage's shader kind; the other stays the inert
 * default (a render-pass stage never touches EncodeCompute, and vice versa). */
#ifndef FBDRAWSTAGE_H
#define FBDRAWSTAGE_H

#include <webgpu/webgpu_cpp.h>
#include "FBGpu.h"
#include "FBFrameContext.h"

namespace FlightBox {

class FBDrawStage {
public:
  virtual ~FBDrawStage() = default;

  virtual void Init(const FBGpu &gpu) { (void)gpu; }
  virtual void Resize(int width, int height) { (void)width; (void)height; }

  virtual void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) { (void)ctx; (void)pass; }
  virtual void EncodeCompute(const FBFrameContext &ctx, wgpu::ComputePassEncoder &pass) { (void)ctx; (void)pass; }
};

} // namespace FlightBox
#endif
