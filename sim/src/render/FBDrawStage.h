/* One shader's pipeline(s) + bind group(s) + draw(s), recorded into a BORROWED encoder FBRenderer
 * already opened. A STAGE NEVER BEGINS OR ENDS A PASS: the pass topology and encode order are
 * FBRenderer's, and a stage split must not add a single render or compute pass.
 * A stage SELF-GATES its own draw ("nothing visible" -> Encode records nothing) rather than the caller
 * deciding, because FBRenderer calls every stage in its slot unconditionally. Override whichever
 * Encode() form matches the shader kind; the other stays the inert default.
 * Vertrag + vollstaendige Encode-Reihenfolge: doc/render/renderer.md, Abschnitt 2. */
#ifndef FBDRAWSTAGE_H
#define FBDRAWSTAGE_H

#include <webgpu/webgpu_cpp.h>
#include "FBGpu.h"
#include "FBFrameContext.h"

namespace FlightBox::Render {

class FBDrawStage {
public:
  virtual ~FBDrawStage() = default;

  virtual void Init(const FBGpu &gpu) { (void)gpu; }
  virtual void Resize(int width, int height) { (void)width; (void)height; }

  virtual void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) { (void)ctx; (void)pass; }
  virtual void EncodeCompute(const FBFrameContext &ctx, wgpu::ComputePassEncoder &pass) { (void)ctx; (void)pass; }
};

} // namespace FlightBox::Render
#endif
