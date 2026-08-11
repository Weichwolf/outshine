/* The casting half of the cascaded shadow maps. Depth only, one pipeline, one draw per cascade into
 * a strip atlas. The receiving half is ShadowSample.h.
 *
 * Casters are the building prisms AND the terrain. Terrain rides the same pipeline because both
 * vertex strides are 32 bytes with position at 0; the per-tile origin is a dynamic uniform offset,
 * which is why the bind group layout is explicit. */
#ifndef SHADOWSTAGE_H
#define SHADOWSTAGE_H

#include <cstdint>
#include <vector>
#include "ClusterDag.h"
#include "DrawStage.h"
#include "ShadowSample.h"

namespace outshine::Render {

class ShadowStage : public DrawStage {
public:
  void Init(const Gpu &gpu) override;

  /* Borrowed from BuildingDraw: the same vertex buffer the scene pass draws, never a second copy,
   * and its DAG so each cascade can take its own cut. */
  void SetCasters(wgpu::Buffer vtx, wgpu::Buffer idx, uint32_t nverts, const DagCluster *clusters,
                  int nclusters, const double anchor[3]);

  /* The terrain half. Each tile is its own buffer with its own origin, so it rides the SAME pipeline
   * (both strides are 32 bytes with position at 0) and differs only in the per-draw offset. */
  /* `Clusters` is BORROWED from the tile table for the length of the frame — residency only changes
   * in World::Update, which has already run when Renderer collects the casters. */
  struct TerrainCaster { wgpu::Buffer Vtx, Idx; uint32_t NVerts, NIdx; const DagCluster *Clusters;
                         int NClusters; double Origin[3]; float BoundCtr[3], BoundRad; };
  void SetTerrainCasters(const std::vector<TerrainCaster> &tiles) { Terrain = tiles; }

  /* Rebuilds the four cascade projections around this frame's camera and sun. Renderer calls it
   * before the shadow pass, because the same matrices go into every receiver's uniform. */
  void Update(const FrameContext &ctx);

  void Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

  wgpu::TextureView AtlasView(void) const { return Atlas.CreateView(); }
  wgpu::Sampler CompareSampler(void) const { return Cmp; }
  /* kShadowUniFloats of cascade matrices + far radii + params, as ShadowSample.h's `Csm`. */
  const float *CsmUniform(void) const { return Csm; }
  bool Active(void) const { return NVerts > 0; }
  /* Triangles the LAST Encode submitted over ALL cascades — the atlas is the frame's largest single
   * geometry consumer, so it needs its own number and not a share of the scene pass's. */
  long TriangleCount(void) const { return DrawnVerts / 3; }
  int DrawCallCount(void) const { return DrawCalls; }

private:
  static constexpr uint32_t kCascadeStride = 256;   /* minUniformBufferOffsetAlignment */
  /* [SET] 2.0 SHADOW TEXELS, and it is coarser than the camera's 1 px for a reason the receiver
   * states: ShadowSample.h filters over a 3x3 grid of hardware-PCF taps, so the penumbra it draws is
   * already blurred across ~4 texels and a caster error below that cannot move an edge in the
   * picture. Nanite does the same thing under the name of a shadow LOD bias. */
  static constexpr double kShadowTauTexels = 2.0;

  void Cut(int cascade, const double rel0[3], const double C[3], const double X[3], const double Y[3],
           const double L[3], double R, double zn, double zf);

  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::RenderPipeline Pipe;
  wgpu::Texture Atlas;
  wgpu::Sampler Cmp;
  wgpu::Buffer Uni;
  wgpu::BindGroupLayout Bgl;
  wgpu::BindGroup Bind;                     /* ONE group; the block is chosen by a dynamic offset */
  std::vector<TerrainCaster> Terrain;
  std::vector<uint32_t> TerrainCut[kShadowCascades];   /* indices into Terrain, per cascade */
  struct TerrainRange { uint32_t Cut, First, Count; };  /* Cut = position in TerrainCut, = uniform block */
  std::vector<TerrainRange> TerrainRanges[kShadowCascades];
  /* [SET] 256 tiles. The reference block is 54; the cap is what bounds the uniform buffer, and a cut
   * that would exceed it is TRUNCATED and counted, never silently dropped. */
  static constexpr uint32_t kMaxShadowTiles = 256;

  wgpu::Buffer Vtx, Idx;
  uint32_t NVerts = 0;
  const DagCluster *Clusters = nullptr;
  int NClusters = 0;
  double Anchor[3] = {0, 0, 0};
  float Csm[kShadowUniFloats] = {};
  struct DrawRange { uint32_t First, Count; };
  std::vector<DrawRange> Ranges[kShadowCascades];   /* reused, so a steady scene allocates nothing */
  long DrawnVerts = 0;
  int DrawCalls = 0;
  long TerrainOverflow = 0;
};

} // namespace outshine::Render
#endif
