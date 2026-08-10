/* ONE TREE, ONE LIGHT, A LADDER OF REPRESENTATIONS. The bark is a closed indexed mesh; the crown is a
 * cluster CARD that cuts the declared lamina outline out of itself in the fragment; the far rank is
 * one camera-facing quad per stand off a runtime-baked octahedral atlas. They are pipelines and not
 * stages because they are one subject: they share the uniform, the stand array, the shading
 * declaration and the pass slot, and splitting them would put the same tree in two places.
 *
 * WHICH RANK A STAND GETS IS SCREEN-SPACE ERROR and nothing else: the stands
 * arrive sorted by distance, so a rank is a contiguous RANGE and the split is a binary search.
 *
 * The mesh arrives as RAW ARRAYS, never as world/TreeMesh: render/ draws what it is handed and does
 * not know how a species grows. Self-gates on "no bark uploaded", so every client that never called
 * SetStand pays nothing.
 *
 * NOT A SECOND LIGHTING MODEL: every fragment splices SurfaceLight.h and binds the same SceneLight
 * bundle every lit surface binds. The one term the opaque BRDF has no place for is a thin leaf's
 * TRANSMISSION, and it carries no new constant — see the WGSL. */
#ifndef TREESTAGE_H
#define TREESTAGE_H

#include <cstdint>
#include <vector>

#include "DrawStage.h"
#include "TreeLook.h"
#include "TreeRanks.h"

namespace outshine::Render {

class TreeStage : public DrawStage {
public:
  void Configure(const Gpu &gpu, const SceneLight &light);

  /* pos3 + nrm3 + uv2 + tan3 per vertex, in NORMALISED tree space (foot at y = 0, height 1).
   * One mesh per rank, grown at `TreeRank::Pixel(rank)`. */
  void SetBark(int rank, const float *verts, uint32_t nverts, const uint32_t *idx, uint32_t nidx);
  /* The single lamina (pos3 + nrm3 + uv2, leaf-local units) plus its instances (pos3 + roll + dir3 +
   * pad). `scaleM` is metres per leaf-local unit. THE SUBJECT BENCH'S leaf: one stand, true geometry. */
  void SetLeaf(const float *verts, uint32_t nverts, const uint32_t *idx, uint32_t nidx,
               const float *inst, uint32_t ninst, float scaleM);
  /* THE FIELD'S CROWN. Same instance layout as SetLeaf's, but every instance is a quad carrying
   * kLeavesPerCard laminae of `leafLenM` rooted along a shoot and fanned over `spreadDeg` — the
   * cluster card every engine draws a canopy with. One set per rank: a card's own size on screen is
   * held constant across the ladder, so the count quarters as the rank's edge doubles and the leaf
   * the card draws grows to keep the declared leaf area index. */
  void SetCards(int rank, const float *inst, uint32_t n, float leafLenM, float spreadDeg);
  void SetLook(const TreeLook &look) { Look = look; }
  /* Where the tree stands, in ground metres east/north of the CAMERA. `heightM` <= 0 retires the
   * whole stage. */
  void SetStand(double eastM, double northM, double eyeAglM, double heightM);
  /* THE STAND, MANY TIMES OVER, SORTED NEAR TO FAR. Five floats per tree: east, north, the foot over
   * the eye in metres, the yaw its own hash gave it and the factor on the species' height. `distM`
   * is the same array's distance from the eye and lives on the CPU only — it is what turns a rank
   * into a range. */
  void SetStands(const float *inst, uint32_t n, const float *distM);
  /* The grown mesh's box in normalised tree units: what an orthographic bake has to frame and what a
   * billboard has to be the size of. */
  void SetCrown(float halfWidth, float top, float bottom);
  void SetSun(const double sunEcef[3], float nightAmbient);
  void SetLeavesVisible(bool on) { LeavesOn = on; }

  /* THE IMPOSTOR IS A CACHE OF A COMPUTABLE FUNCTION and is baked from the grown mesh at load, never
   * shipped: kCells x kCells hemi-octahedral views of albedo+coverage and of the tree-space normal,
   * so the far rank is lit by the same SurfaceLight the near rank is. Renderer owns the pass; this
   * allocates the targets and records the cells into it. */
  bool WantsBake() const { return BarkCount[0] > 0 && !ImpAlbedo; }
  void CreateImpostor();
  void EncodeBake(wgpu::RenderPassEncoder &pass);
  void FinishBake();
  /* The BAKE's attachments, not the bind group's: an atlas cannot be a render target and a sampled
   * texture in the same synchronisation scope, so the group keeps its 1x1 until FinishBake. */
  wgpu::TextureView ImpostorAlbedoTarget() const { return ImpAlbedo.CreateView(); }
  wgpu::TextureView ImpostorNormalTarget() const { return ImpNormal.CreateView(); }
  wgpu::TextureView ImpostorDepthTarget() const { return ImpDepthView; }
  static constexpr int kCells = 8;
  static constexpr int kCellSize = 256;

  void Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

  long TriangleCount() const { return Drawn; }
  long MeshStands() const { return MeshN; }
  double MeshRadiusM() const { return MeshRadius; }
  long ImpostorStands() const { return ImpN; }
  long RankStands(int k) const { return RankN[k]; }

private:
  static constexpr int kUniFloats = 64;
  static constexpr int kBarkFloats = 11;
  static constexpr int kLeafFloats = 8;
  static constexpr int kInstFloats = 8;
  static constexpr int kStandFloats = 5;

  wgpu::Buffer Upload(const void *data, size_t bytes, wgpu::BufferUsage usage);
  void Rebind();

  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::RenderPipeline BarkPipe, LeafPipe, CardPipe, ImpPipe, BarkBakePipe, CardBakePipe;
  wgpu::BindGroupLayout Bgl;
  wgpu::BindGroup Bind, BakeBind;
  wgpu::Buffer Uni, BakeUni, LeafVtx, LeafIdx, LeafInst, CardInst, StandBuf;
  wgpu::Buffer BarkVtx[TreeRank::kCount], BarkIdx[TreeRank::kCount];
  wgpu::Texture ImpAlbedo, ImpNormal, ImpDepth;
  wgpu::TextureView ImpAlbedoView, ImpNormalView, ImpDepthView;
  wgpu::Sampler ImpSamp;
  SceneLight Light;

  TreeLook Look;
  std::vector<float> StandDist;
  std::vector<float> CardStage[TreeRank::kCount];
  double SunDir[3] = {0, 0, 1};
  double EastM = 0.0, NorthM = 0.0, EyeAglM = 0.0, HeightM = 0.0;
  float LeafScaleM = 0.1f;
  float CardLeafM[TreeRank::kCount] = {0.2f, 0.2f, 0.2f};
  float CardSpreadDeg = 110.0f;
  float NightAmbient = 0.0f;
  uint32_t BarkCount[TreeRank::kCount] = {}, CardCount[TreeRank::kCount] = {}, CardBase[TreeRank::kCount] = {};
  uint32_t LeafCount = 0, InstCount = 0, StandCount = 0;
  float CrownHalf = 0.35f, CrownTop = 1.0f, CrownBot = 0.0f;
  bool LeavesOn = true;
  long Drawn = 0, MeshN = 0, ImpN = 0;
  long RankN[TreeRank::kCount] = {};
  double MeshRadius = 0.0;
};

} // namespace outshine::Render
#endif
