/* ONE TREE, TWO NETS, ONE LIGHT. The bark is a closed indexed mesh; the foliage is the species' single
 * lamina, instanced once per declared leaf slot. They are two pipelines and not two stages because
 * they are one subject: they share the uniform, the stand, the shading declaration and the pass slot,
 * and splitting them would put the same tree in two places for a reader to keep in step.
 *
 * The mesh arrives as RAW ARRAYS, never as world/TreeMesh: render/ draws what it is handed and does
 * not know how a species grows. Self-gates on "no bark uploaded", so every client that never called
 * SetStand pays nothing.
 *
 * NOT A SECOND LIGHTING MODEL: both fragments splice SurfaceLight.h and bind the same SceneLight
 * bundle every lit surface binds. The one term the opaque BRDF has no
 * place for is a thin leaf's TRANSMISSION, and it carries no new constant — see the WGSL. */
#ifndef TREESTAGE_H
#define TREESTAGE_H

#include <cstdint>

#include "DrawStage.h"

namespace outshine::Render {

/* The species' shading declaration, as render/ needs it. `BarkFreq` is furrow cycles per RADIAN of
 * circumference and `LeafRgb` is already tint x base — the multiplication is world knowledge. */
struct TreeLook {
  float BarkRgb[3] = {0.40f, 0.31f, 0.23f};
  float BarkDark = 0.62f;
  float BarkFreq = 4.0f;
  float BarkRidge = 0.2f;
  float LeafRgb[3] = {0.068f, 0.107f, 0.027f};
};

class TreeStage : public DrawStage {
public:
  void Configure(const Gpu &gpu, const SceneLight &light);

  /* pos3 + nrm3 + uv2 + tan3 per vertex, in NORMALISED tree space (base at y = 0, height 1). */
  void SetBark(const float *verts, uint32_t nverts, const uint32_t *idx, uint32_t nidx);
  /* The single lamina (pos3 + nrm3 + uv2, leaf-local units) plus its instances (pos3 + roll + dir3 +
   * pad). `scaleM` is metres per leaf-local unit. */
  void SetLeaf(const float *verts, uint32_t nverts, const uint32_t *idx, uint32_t nidx,
               const float *inst, uint32_t ninst, float scaleM);
  void SetLook(const TreeLook &look) { Look = look; }
  /* Where the tree stands, in ground metres east/north of the CAMERA — the frame BenchGroundStage's
   * plane and card already use. `heightM` <= 0 retires the whole stage. */
  void SetStand(double eastM, double northM, double eyeAglM, double heightM);
  /* THE STAND, MANY TIMES OVER. Four floats per tree: east, north, height in metres and the yaw its
   * own hash gave it. One mesh, one draw, N instances — what every engine does with vegetation. */
  void SetStands(const float *inst, uint32_t n);
  void SetSun(const double sunEcef[3], float nightAmbient);
  void SetLeavesVisible(bool on) { LeavesOn = on; }

  void Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

  long TriangleCount() const { return Drawn; }

private:
  static constexpr int kUniFloats = 48;   /* mat4 + eight vec4f — the WGSL `T` verbatim */
  static constexpr int kBarkFloats = 11;
  static constexpr int kLeafFloats = 8;
  static constexpr int kInstFloats = 8;

  wgpu::Buffer Upload(const void *data, size_t bytes, wgpu::BufferUsage usage);

  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::RenderPipeline BarkPipe, LeafPipe;
  wgpu::BindGroup Bind;
  wgpu::Buffer Uni, BarkVtx, BarkIdx, LeafVtx, LeafIdx, LeafInst;
  SceneLight Light;

  TreeLook Look;
  double SunDir[3] = {0, 0, 1};
  double EastM = 0.0, NorthM = 0.0, EyeAglM = 0.0, HeightM = 0.0;
  float LeafScaleM = 0.1f;
  float NightAmbient = 0.0f;
  uint32_t BarkCount = 0, LeafCount = 0, InstCount = 0, StandCount = 0;
  wgpu::Buffer StandBuf, OneStand;
  bool LeavesOn = true;
  long Drawn = 0;
};

} // namespace outshine::Render
#endif
