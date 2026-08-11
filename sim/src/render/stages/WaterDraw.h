/* OSM water surfaces, drawn as their own geometry over the declared SECOND vertex layout
 * (`pos3 + nrm3`, 24 B) — no uv exists on a surface whose material is one number. Self-gates on "no
 * mesh", so a client that never calls SetMesh pays nothing. */
#ifndef WATERDRAW_H
#define WATERDRAW_H

#include <cstdint>

#include "GeometryUnit.h"
#include "Gpu.h"

namespace outshine::Render {

class WaterDraw : public GeometryUnit {
public:
  void Configure(const Gpu &gpu, const SceneLight &light);

  /* pos3 + nrm3 per vertex, positions as ECEF offsets from `anchor`. */
  void SetMesh(const float *verts, uint32_t nverts, const double anchor[3]);
  void SetSun(const double sunEcef[3], const double up[3], float nightAmbient);

  void Encode(const FrameContext &ctx, ClusterCut &cut, wgpu::RenderPassEncoder &pass) override;

  uint32_t VertexCount() const { return NVerts; }

private:
  static constexpr int kUniFloats = 28;   /* mat4 + anc + sun + up — the WGSL `W` verbatim */

  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
  wgpu::Buffer Uni, Vtx;
  SceneLight Light;
  uint32_t NVerts = 0;
  double Anchor[3] = {0, 0, 0}, SunDir[3] = {0, 0, 1}, Up[3] = {0, 0, 1};
  float NightAmbient = 0.0f;
};

} // namespace outshine::Render
#endif
