/* THE DECLARED SUBJECT OF A STUDIO: one indexed triangle mesh, drawn where the declaration puts it
 * and shaded by nothing at all.
 *
 * POSITION-ONLY, AND THAT IS THE WHOLE POINT OF THE UNIT. What it answers is *which pixels the
 * geometry covers and how far away it is* -- the two questions the coverage and depth rungs ask
 * (doc/requirements.md I.26), neither of which reads a normal, a uv or a material. Admitting a
 * normal here would mean either inventing one for a subject whose file carries none, which I.26
 * forbids, or refusing every such subject; the third answer is that a depth-and-coverage pipeline
 * has no use for one. A rung that compares RADIANCE needs a mesh that carries what radiance reads,
 * and that is a different unit and a different round.
 *
 * NO BACK-FACE CULLING. A path tracer's camera ray hits a single-sided triangle from either side, so
 * culling here would compare two different predicates; winding is rung 3's subject and is decided
 * where a material exists to decide it. */
#ifndef SUBJECTDRAW_H
#define SUBJECTDRAW_H

#include <cstdint>

#include "GeometryUnit.h"
#include "Gpu.h"

namespace outshine::Render {

class SubjectDraw : public GeometryUnit {
public:
  void Configure(const Gpu &gpu);

  /* `verts` is 3 floats per vertex, ECEF offsets from `anchor` in metres; `idx` indexes them.
   * A zero count retires the unit, which is the state every client that never declares a subject
   * stays in for the whole of its life. */
  void SetMesh(const float *verts, uint32_t nverts, const uint32_t *idx, uint32_t nidx,
               const double anchor[3]);

  void Encode(const FrameContext &ctx, ClusterCut &cut, wgpu::RenderPassEncoder &pass) override;

  uint32_t VertexCount() const { return NVerts; }
  long TriangleCount() const { return (long)NIdx / 3; }

private:
  static constexpr int kUniFloats = 20; /* mat4 + anc -- the WGSL struct `S` verbatim */

  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
  wgpu::Buffer Uni, Vtx, Idx;
  uint32_t NVerts = 0, NIdx = 0;
  double Anchor[3] = {0, 0, 0};
};

} // namespace outshine::Render
#endif
