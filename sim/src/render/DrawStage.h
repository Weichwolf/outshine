/* One shader's pipeline(s) + bind group(s) + draw(s), recorded into a BORROWED encoder Renderer
 * already opened. A STAGE NEVER BEGINS OR ENDS A PASS: the pass topology and encode order are
 * Renderer's, and a stage split must not add a single render or compute pass.
 * A stage SELF-GATES its own draw ("nothing visible" -> Encode records nothing) rather than the caller
 * deciding, because Renderer calls every stage in its slot unconditionally. Override whichever
 * Encode() form matches the shader kind; the other stays the inert default. */
#ifndef DRAWSTAGE_H
#define DRAWSTAGE_H

#include <webgpu/webgpu_cpp.h>
#include "Gpu.h"
#include "FrameContext.h"

namespace outshine::Render {

class DrawStage {
public:
  virtual ~DrawStage() = default;

  virtual void Init(const Gpu &gpu) { (void)gpu; }
  virtual void Resize(int width, int height) { (void)width; (void)height; }

  virtual void Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) { (void)ctx; (void)pass; }
  virtual void EncodeCompute(const FrameContext &ctx, wgpu::ComputePassEncoder &pass) { (void)ctx; (void)pass; }
};

} // namespace outshine::Render
#endif
