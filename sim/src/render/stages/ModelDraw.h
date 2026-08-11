/* ONE MODEL, ONE LIGHT, A LADDER OF REPRESENTATIONS, drawn at every instance that claimed space.
 * A level is a closed indexed mesh plus a set of instanced SHEETS — quads that cut a declared
 * outline out of themselves in the fragment — and the level above the last one is a camera-facing
 * quad off a runtime-baked octahedral atlas. They are pipelines and not units because they are one
 * subject: they share the uniform, the instance array, the material row and the draw slot.
 *
 * WHICH LEVEL AN INSTANCE GETS IS SCREEN-SPACE ERROR and nothing else, taken from the same ladder
 * every cluster answers (core/ClusterDag.h, core/ModelLadder.h): a level's model-space error is a
 * fraction of the model's own height, and the cut is `DagSelect` with the root removed because an
 * instance is a point.
 *
 * THE ENGINE KNOWS NO CONTENT. What arrives here is a prototype, its levels, its instances and a
 * MATERIAL ROW of numbers whose meanings are pinned in the shader that reads them — never a named
 * part of whatever the model happens to be.
 *
 * NOT A SECOND LIGHTING MODEL: every fragment splices SurfaceLight.h and binds the same SceneLight
 * bundle every lit surface binds. The one term the opaque BRDF has no place for is a thin sheet's
 * TRANSMISSION, and it carries no new constant — see the WGSL. */
#ifndef MODELDRAW_H
#define MODELDRAW_H

#include <cstdint>
#include <vector>

#include "ChunkVtx.h"
#include "GeometryUnit.h"
#include "Gpu.h"
#include "Material.h"
#include "ModelLadder.h"
#include "TangentFrame.h"

namespace outshine::Render {

/* One level's solid mesh: core/ChunkVtx.h's PlainVtx (24 B) in NORMALISED model space — foot at
 * y = 0, height 1. */
struct LevelMesh {
  const float *Verts = nullptr;
  uint32_t VertCount = 0;
  const uint32_t *Idx = nullptr;
  uint32_t IdxCount = 0;
};

/* One level's instanced sheets. Each quad stands for `kElementsPerSheet` cut-out elements rooted
 * along a shoot line and fanned over `FanDeg`; eight floats per sheet — anchor3 + roll, then
 * direction3 + pad. A sheet's own size on screen is held constant across the ladder, so the count
 * quarters as the level's error doubles and `SizeM` grows to keep the declared area. */
struct SheetSet {
  const float *Sheets = nullptr;
  uint32_t Count = 0;
  float SizeM = 0.0f;    /* one element's length in metres at the model's declared height */
  float FanDeg = 0.0f;
};

/* THE SUBJECT BENCH'S TRUE GEOMETRY: one sheet mesh (pos3 + nrm3 + uv2) instanced over declared
 * placements (pos3 + roll, dir3 + pad). `ScaleM` is metres per sheet-local unit. */
struct DetailMesh {
  const float *Verts = nullptr;
  uint32_t VertCount = 0;
  const uint32_t *Idx = nullptr;
  uint32_t IdxCount = 0;
  const float *Instances = nullptr;
  uint32_t InstanceCount = 0;
  float ScaleM = 0.1f;
};

/* The model's box in units of its own height: what an orthographic bake has to frame and what a
 * billboard has to be the size of. */
struct ModelBounds {
  float HalfWidth = 0.35f;
  float Top = 1.0f;
  float Bottom = 0.0f;
};

class ModelDraw : public GeometryUnit {
public:
  void Configure(const Gpu &gpu, const SceneLight &light);

  void SetLevel(int level, const LevelMesh &mesh);
  void SetSheets(int level, const SheetSet &sheets);
  void SetDetail(const DetailMesh &detail);
  void SetMaterial(const float row[kMaterialRowFloats]);
  /* The model's own height in metres — the scale every instance's factor multiplies. <= 0 with no
   * instance field retires the whole unit. */
  void SetHeightM(double heightM);
  void SetBounds(const ModelBounds &bounds);
  /* Where ONE instance stands, in ground metres east/north of the camera: the subject bench's
   * placement, and the state no other client leaves. */
  void SetSubject(double eastM, double northM, double eyeAglM);
  /* THE INSTANCE FIELD, IN NO ORDER. Five floats per instance: east and north metres in `frame`,
   * the foot's height over that frame's anchor, the yaw and the factor on the model's height. The
   * frame is a PLACE — a field measured from the eye would travel with it — and which level an
   * instance draws at is this unit's own answer, per frame. */
  void SetInstances(const float *instances, uint32_t n, const TangentFrame &frame);
  void SetSun(const double sunEcef[3], float nightAmbient);
  void SetSheetsVisible(bool on) { SheetsOn = on; }

  /* THE IMPOSTOR IS A CACHE OF A COMPUTABLE FUNCTION and is baked from the grown mesh at load,
   * never shipped: kCells x kCells hemi-octahedral views of albedo+coverage and of the model-space
   * normal, so the far level is lit by the same SurfaceLight the near one is. Renderer owns the
   * pass; this allocates the targets and records the cells into it. */
  bool WantsBake() const { return LevelIdxCount[0] > 0 && !ImpAlbedo; }
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

  void Encode(const FrameContext &ctx, ClusterCut &cut, wgpu::RenderPassEncoder &pass) override;

  long TriangleCount() const { return Drawn; }
  long MeshInstances() const { return MeshN; }
  double MeshRadiusM() const { return MeshRadius; }
  long ImpostorInstances() const { return ImpN; }
  long LevelInstances(int level) const { return LevelN[level]; }

private:
  static constexpr int kUniFloats = 64;
  static constexpr int kLevels = ModelLadder::kLevels;
  static constexpr int kSolidFloats = (int)(kPlainVertexStrideB / sizeof(float));
  static constexpr int kDetailFloats = 8;
  static constexpr int kInstFloats = 8;
  static constexpr int kPlaceFloats = 5;
  /* The uniform's own field offsets, in floats. The WGSL struct `M` is the other half of this
   * statement and the only place the meanings are written down. */
  static constexpr int kUniFrame = 16, kUniSun = 28, kUniLevel = 32, kUniCut = 36, kUniBox = 40,
                       kUniMaterial = 44;

  /* One frame's answer for one level: where its instances start in the order buffer and how many
   * there are. */
  struct LevelCut {
    uint32_t First = 0, Count = 0;
  };

  wgpu::Buffer Upload(const void *data, size_t bytes, wgpu::BufferUsage usage);
  void Rebind();
  /* The instance frame relative to THIS eye, and the model origin in it. */
  void Aim(const FrameContext &ctx, double east[3], double north[3], double up[3],
           double origin[3]) const;
  /* Sorts the instances into levels by the ladder's own inequality and returns how many stayed on
   * a mesh level; the rest are the impostor's. */
  uint32_t Sort(const ClusterCut &cut, const double east[3], const double north[3],
                const double up[3], const double origin[3], LevelCut cuts[kLevels]);
  void WriteUniforms(const FrameContext &ctx, const double east[3], const double north[3],
                     const double up[3], const double origin[3], const LevelCut cuts[kLevels],
                     uint32_t meshN, uint32_t offsets[kLevels + 1]);
  void DrawSolids(wgpu::RenderPassEncoder &pass, const LevelCut cuts[kLevels],
                  const uint32_t offsets[kLevels + 1]);
  void DrawSheets(wgpu::RenderPassEncoder &pass, const LevelCut cuts[kLevels],
                  const uint32_t offsets[kLevels + 1], uint32_t meshN);

  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::RenderPipeline SolidPipe, DetailPipe, SheetPipe, ImpPipe, SolidBakePipe, SheetBakePipe;
  wgpu::BindGroupLayout Bgl;
  wgpu::BindGroup Bind, BakeBind;
  wgpu::Buffer Uni, BakeUni, DetailVtx, DetailIdx, DetailInst, SheetBuf, PlaceBuf, OrderBuf;
  wgpu::Buffer LevelVtx[kLevels], LevelIdx[kLevels];
  wgpu::Texture ImpAlbedo, ImpNormal, ImpDepth;
  wgpu::TextureView ImpAlbedoView, ImpNormalView, ImpDepthView;
  wgpu::Sampler ImpSamp;
  SceneLight Light;

  float MaterialRow[kMaterialRowFloats] = {};
  ModelBounds Bounds;
  TangentFrame Frame;
  std::vector<float> Places;
  std::vector<uint32_t> Order;
  std::vector<uint8_t> LevelOf;
  std::vector<float> SheetStage[kLevels];
  double SunDir[3] = {0, 0, 1};
  double EastM = 0.0, NorthM = 0.0, EyeAglM = 0.0, HeightM = 0.0;
  float DetailScaleM = 0.1f;
  float SheetSizeM[kLevels] = {0.2f, 0.2f, 0.2f, 0.2f};
  float SheetFanDeg = 110.0f;
  float NightAmbient = 0.0f;
  uint32_t LevelIdxCount[kLevels] = {}, SheetCount[kLevels] = {}, SheetBase[kLevels] = {};
  uint32_t DetailIdxCount = 0, DetailInstCount = 0, PlaceCount = 0;
  bool SheetsOn = true;
  long Drawn = 0, MeshN = 0, ImpN = 0;
  long LevelN[kLevels] = {};
  double MeshRadius = 0.0;
};

} // namespace outshine::Render
#endif
