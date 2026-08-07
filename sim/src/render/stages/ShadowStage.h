/* The casting half of the cascaded shadow maps. Depth only, one pipeline, one draw per cascade into
 * a strip atlas — so ONE render pass covers all four, and the pass boundary is Renderer's as always.
 * The receiving half is ShadowSample.h, which every lit surface splices.
 *
 * Casters are the OSM building prisms. Terrain does not cast this round: the tile draw is a render
 * bundle of hundreds of per-tile buffers with no per-cascade cull, so re-drawing it four times would
 * cost more than the ridge shadows it would buy at a pedestrian's sun angles. Named as a gap rather
 * than hidden. */
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

  /* Borrowed from BuildingsStage: the same vertex buffer the scene pass draws, never a second copy,
   * and its DAG so each cascade can take its own cut. */
  void SetCasters(wgpu::Buffer vtx, uint32_t nverts, const DagCluster *clusters, int nclusters,
                  const double anchor[3]);

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
  wgpu::BindGroup Bind[kShadowCascades];

  wgpu::Buffer Vtx;
  uint32_t NVerts = 0;
  const DagCluster *Clusters = nullptr;
  int NClusters = 0;
  double Anchor[3] = {0, 0, 0};
  float Csm[kShadowUniFloats] = {};
  struct DrawRange { uint32_t First, Count; };
  std::vector<DrawRange> Ranges[kShadowCascades];   /* reused, so a steady scene allocates nothing */
  long DrawnVerts = 0;
  int DrawCalls = 0;
};

} // namespace outshine::Render
#endif
