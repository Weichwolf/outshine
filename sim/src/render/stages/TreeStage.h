/* ONE TREE, ONE LIGHT, A LADDER OF REPRESENTATIONS. The bark is a closed indexed mesh; the crown is a
 * cluster CARD that cuts the declared lamina outline out of itself in the fragment; the far rank is
 * one camera-facing quad per stand off a runtime-baked octahedral atlas. They are pipelines and not
 * stages because they are one subject: they share the uniform, the stand array, the shading
 * declaration and the pass slot, and splitting them would put the same tree in two places.
 *
 * WHICH RANK A STAND GETS IS SCREEN-SPACE ERROR and nothing else (doc/render/lod.md): the stands
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

namespace outshine::Render {

/* The species' shading declaration, as render/ needs it. `BarkFreq` is furrow cycles per RADIAN of
 * circumference and `LeafRgb` is already tint x base — the multiplication is world knowledge. The
 * `Leaf*` block is world/TreeLeaf's `ProfileWidth` verbatim, so the card's silhouette is the declared
 * lamina and no atlas has to carry a leaf shape. */
struct TreeLook {
  float BarkRgb[3] = {0.40f, 0.31f, 0.23f};
  float BarkDark = 0.62f;
  float BarkFreq = 4.0f;
  float BarkRidge = 0.2f;
  float LeafRgb[3] = {0.068f, 0.107f, 0.027f};
  float LeafWidth = 0.34f;
  float LeafWidest = 0.45f;
  float LeafTip = 0.5f;
  float LeafBaseFill = 0.0f;
  float LeafLobes = 0.0f;
  float LeafLobeDepth = 0.0f;
  float LeafSerration = 0.0f;
  float LeafFold = 0.10f;
  float NeedleWidth = 0.0f;   /* > 0 selects the needle profile */
};

class TreeStage : public DrawStage {
public:
  static constexpr int kLeavesPerCard = 16;   /* the WGSL `kCardLeaves` — change one, change both */
  /* THE MESH RANKS, and their number is the only thing about them that is chosen. Everything else
   * follows from `kCellPx`: rank k's mesh may be as coarse as one pixel at its NEAREST stand, its
   * near edge is where the rank below hands over, and the last one hands over to the impostor at
   * `kCellPx`. `RankPixel(k)` and `RankEdge(k)` are that ladder. */
  static constexpr int kRanks = 4;
  /* THE IMPOSTOR'S TEXEL IS THE MODEL-SPACE ERROR every mesh rank is measured against: a tree of
   * height H baked into a cell of this many pixels carries an error of H/kCellPx metres, and that
   * projects to one pixel at d = H * f_px / kCellPx. That inequality — doc/render/lod.md's, with
   * lambda = H/kCellPx — is the ONLY thing that decides where the mesh stops. */
  static constexpr float kCellPx = 256.0f;
  /* One pixel at rank k's NEAREST stand, as a fraction of the tree's height — the grower's whole
   * detail input. The impostor's cell is the anchor and every rank below it halves. */
  static constexpr float RankPixel(int k) {
    return 1.0f / (kCellPx * (float)(1u << (unsigned)(kRanks - k)));
  }

  void Configure(const Gpu &gpu, const SceneLight &light);

  /* pos3 + nrm3 + uv2 + tan3 per vertex, in NORMALISED tree space (foot at y = 0, height 1).
   * One mesh per rank, grown at `RankPixel(rank)`. */
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
  wgpu::Buffer BarkVtx[kRanks], BarkIdx[kRanks];
  wgpu::Texture ImpAlbedo, ImpNormal, ImpDepth;
  wgpu::TextureView ImpAlbedoView, ImpNormalView, ImpDepthView;
  wgpu::Sampler ImpSamp;
  SceneLight Light;

  TreeLook Look;
  std::vector<float> StandDist;
  std::vector<float> CardStage[kRanks];
  double SunDir[3] = {0, 0, 1};
  double EastM = 0.0, NorthM = 0.0, EyeAglM = 0.0, HeightM = 0.0;
  float LeafScaleM = 0.1f;
  float CardLeafM[kRanks] = {0.2f, 0.2f, 0.2f};
  float CardSpreadDeg = 110.0f;
  float NightAmbient = 0.0f;
  uint32_t BarkCount[kRanks] = {}, CardCount[kRanks] = {}, CardBase[kRanks] = {};
  uint32_t LeafCount = 0, InstCount = 0, StandCount = 0;
  float CrownHalf = 0.35f, CrownTop = 1.0f, CrownBot = 0.0f;
  bool LeavesOn = true;
  long Drawn = 0, MeshN = 0, ImpN = 0;
  long RankN[kRanks] = {};
  double MeshRadius = 0.0;
};

} // namespace outshine::Render
#endif
