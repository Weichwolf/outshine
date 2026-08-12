/* One kind of surface inside the geometry stage: its pipelines, its bind groups, its draws. A unit
 * is NOT a stage — it neither opens a pass nor makes a cut of its own. It is handed the pass the
 * renderer opened and the cut the stage already made, and it records into both.
 *
 * A unit SELF-GATES ("nothing of mine is visible" -> record nothing), because the stage runs every
 * unit in its slot unconditionally. */
#ifndef GEOMETRYUNIT_H
#define GEOMETRYUNIT_H

#include <webgpu/webgpu_cpp.h>

#include "ClusterCut.h"
#include "FrameContext.h"

namespace outshine::Render {

class GeometryUnit {
public:
  virtual ~GeometryUnit() = default;

  virtual void Encode(const FrameContext &ctx, ClusterCut &cut,
                      wgpu::RenderPassEncoder &pass) = 0;
};

} // namespace outshine::Render
#endif
