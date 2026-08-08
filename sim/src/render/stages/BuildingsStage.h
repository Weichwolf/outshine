/* OSM building prisms. One vertex buffer, one draw, one pipeline — the mesh is static once built, so
 * the only per-frame traffic is the camera-relative anchor in the uniform.
 *
 * The vertex uv is (metres along the wall, metres above the base) rather than 0..1, so the storey
 * lines this shader draws today and the window grid a later one draws are the same two numbers at two
 * levels of detail; neither needs the geometry rebuilt. */
#ifndef BUILDINGSSTAGE_H
#define BUILDINGSSTAGE_H

#include <cstdint>
#include <vector>
#include "DrawStage.h"
#include "ClusterDag.h"

namespace outshine::Render {

class BuildingsStage : public DrawStage {
public:
  void Configure(const Gpu &gpu, const SceneLight &light);

  /* pos3 + norm3 + uv2 per vertex, positions as ECEF offsets from `anchor`. `clusters` is the DAG
   * (doc/render/lod.md); level 0 leads the buffer, which is what lets the shadow cast keep using the
   * finest geometry while the camera pass takes the cut. */
  void SetMesh(const float *verts, uint32_t nverts, const uint32_t *idx, uint32_t nidx,
               const DagCluster *clusters, int nclusters, const double anchor[3]);
  void SetSun(const double sunEcef[3], const double up[3], float nightAmbient);

  void Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

  uint32_t VertexCount() const { return DrawnVerts; }
  /* The caster mesh, kept for the day shadows return: never a second copy, and the DAG comes
   * with them — a shadow view is a VIEW, so it takes its own cut at its own error rather than the
   * whole of level 0. */
  uint32_t CasterVertexCount() const { return BaseVerts; }
  wgpu::Buffer CasterBuffer() const { return Vtx; }
  wgpu::Buffer CasterIndexBuffer() const { return Idx; }
  const double *CasterAnchor() const { return Anchor; }
  const DagCluster *CasterClusters() const { return Clusters.data(); }
  int CasterClusterCount() const { return (int)Clusters.size(); }

private:
  static constexpr int kUniFloats = 28;   /* mat4 + anchor + sun + up — the WGSL `B` verbatim */

  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
  wgpu::Buffer Uni, Vtx, Idx;

  SceneLight Light;
  double Anchor[3] = {0, 0, 0};
  double SunDir[3] = {0, 0, 1};
  double Up[3] = {0, 0, 1};
  float NightAmbient = 0.0f;
  uint32_t NVerts = 0, NIdx = 0, Cap = 0, IdxCap = 0, BaseVerts = 0, DrawnVerts = 0;
  std::vector<DagCluster> Clusters;
  struct DrawRange { uint32_t First, Count; };
  std::vector<DrawRange> Ranges;
};

} // namespace outshine::Render
#endif
