/* The terrain draw — the ONE unit with real per-frame CPU state: the growable albedo array, the
 * RenderBundle, the 2-phase-commit tile table and the mode-strictness invariant counters.
 * Its Configure() must run AFTER Renderer::CreateAtmosphere, whose LUT views it is handed. */
#ifndef TERRAINDRAW_H
#define TERRAINDRAW_H

#include <cstdint>
#include <vector>
#include "CloudDensity.h"
#include "ClusterDag.h"
#include "GeometryUnit.h"
#include "Gpu.h"

namespace outshine::Render {

class TerrainDraw : public GeometryUnit {
public:
  /* `vegTable` may be null: the ground-cover branch is then a baked-const OFF and the shader keeps its
   * raw albedo, which is exactly the picture before the template table existed. */
  void Configure(const Gpu &gpu, wgpu::Sampler lutSamp,
                wgpu::TextureView skyLutView, wgpu::Buffer atmoBuf, int maxLayers,
                wgpu::Buffer vegTable, const SceneLight &light);

  /* THE CLASS STRUCTURE (world/ClassField.h): OSM outlines plus their acceleration grid, evaluated
   * per fragment. `east`/`north` are the ECEF axes of the structure's own origin — the fragment
   * projects its camera-relative offset on THESE axes, so CPU and GPU place a world point identically
   * by construction and not by two formulas that have to agree. */
  void SetClassFrame(const double east[3], const double north[3], const double camOffset[2]);
  void WriteClassBuffer(const uint32_t *words, size_t bytes);

  /* The SAME weather sample the cloud pass marches (Renderer::SetCloudSky hands it to both): the
   * terrain reads its visibility for the haze and its decks for how much sun reaches the ground.
   * Unset = 100 km visibility, no deck — a clear standard atmosphere, not "no atmosphere". */
  void SetSky(const CloudSky &sky) { Sky = sky; }

  /* THE STAND THE GROUND FRAGMENT IS (render/Sward.h), placed on the world graticule so that what
   * grows at a point is a property of the point. `east/north/up` is the ECEF basis at the camera. */
  void SetSward(double lat, double lon, const double east[3], const double north[3],
                const double up[3]);

  /* The declared bare-rock row and how wide the slope transition onto it is
   * (src/assets/world/vegetation.json alpineLimit, world/AlpineLimit.h). Zero band = the fallback is off,
   * which is what a table without the block means. */
  void SetBareRock(int vegRow, float slopeBandDeg) {
    BareRockRow = vegRow; BareSlopeBandDeg = slopeBandDeg;
  }

  /* World drives a mutable per-tile table. Call before Configure(). */

  /* A table slot id, or -1 if the class array is full. The caller has checked DeviceUsable().
   * `clusters` is the tile's DAG: every level lives in the one vertex buffer and
   * the per-frame cut picks the ranges. `anchor` is the ECEF point the procedural surface is measured
   * from; `origin - anchor` must stay small enough for float. */
  int UploadTile(const float *verts, uint32_t nverts, const uint32_t *idx, uint32_t nidx,
                const DagCluster *clusters, int nclusters,
                const double origin[3], const double anchor[3]);
  void ReleaseTile(int slot);
  void SetDrawList(const int *slots, int n) { DrawList.assign(slots, slots + n); }
  /* The classification input. It does not grow with the tile count, because it has nothing to do
   * with tiles — it is the vector geometry itself plus the grid that finds it. */
  long ClassVramBytes(void) const { return ClassBytes; }
  /* The RESIDENT tile geometry, not the drawn share of it: eviction and the frustum move the drawn
   * set every frame, so a figure taken from it is a sample and not the memory that is held. */
  long TileMeshBytes(void) const { return MeshBytes; }
  int DrawCount(void) const { return (int)DrawList.size(); }
  /* Draw CALLS the last Encode recorded, which is not the tile count once the cut merges runs and the
   * frustum drops tiles — the two numbers used to be equal and the budget needs the one that is paid. */
  int DrawCallCount(void) const { return DrawCalls; }
  /* Tiles the frustum kept, out of DrawCount(). */
  int VisibleTileCount(void) const { return VisibleTiles; }
  /* Triangles the LAST Encode actually submitted, not the ones resident: the budget curve is about
   * what the frame paid for. Triangle-list, hence /3. */
  long TriangleCount(void) const { return DrawnVerts / 3; }
  /* WHICH RUNG the cut stands on, per level, in triangles — the only evidence that a tolerance change
   * moved anything at all. */
  const long *TrianglesByLevel(void) const { return DrawnByLevel; }
  static constexpr int kLevelBins = ClusterCut::kLevelBins;

  /* Self-contained: there is always terrain to draw once configured. */
  void Encode(const FrameContext &ctx, ClusterCut &cut, wgpu::RenderPassEncoder &pass) override;

  /* Invariant telemetry, read by Renderer's periodic [present] log. */
  /* The resident tiles as CASTERS: the same buffers the scene pass draws, never a second copy. A
   * shadow cascade is a different view, so it takes no part of the camera's cut — it takes the whole
   * residency, culls against its own box and cuts the DAG at its own tolerance. */
  struct Caster { wgpu::Buffer Vtx, Idx; uint32_t NVerts, NIdx; const DagCluster *Clusters;
                  int NClusters; double Origin[3]; float BoundCtr[3], BoundRad; };
  void CollectCasters(std::vector<Caster> &out) const;

  long GetNotReadyDraws(void) const { return NotReadyDraws; }
  long GetWrongModeDraws(void) const { return WrongModeDraws; }
  long GetBlackDraws(void) const { return BlackDraws; }
  long GetBundleRecords(void) const { return TerrainBundleRecords; }

private:
  SceneLight Light;   /* borrowed: IrradianceStage's buffer */

  /* Bound* is the whole tile over EVERY level, taken off the vertices and not off the DAG spheres. */
  struct DynTile { wgpu::Buffer Vtx, Idx; uint32_t NVerts, NIdx; double Origin[3]; float Anchor[3];
                   bool Used; std::vector<DagCluster> Clusters;
                   float BoundCtr[3]; float BoundRad; };

  static constexpr int kUniFloats = 52;   /* mat4 + sun + haze + stand + class frame; WGSL `U` verbatim */
  static constexpr int kTileFloats = 12;  /* the WGSL `Tile`: three vec4 */

  void RebuildBind(void);

  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::TextureFormat HdrFormat;
  wgpu::Sampler LutSamp;
  wgpu::TextureView SkyLutView;
  wgpu::Buffer AtmoBuf;
  wgpu::Buffer VegBuf;
  CloudSky Sky;
  double SwCellE = 1.0, SwCellN = 1.0, SwFracE = 0.0, SwFracN = 0.0;
  double SwEast[3] = {1, 0, 0}, SwNorth[3] = {0, 1, 0}, SwUp[3] = {0, 0, 1};
  long SwBaseI = 0, SwBaseJ = 0;
  bool SwHave = false;
  int BareRockRow = 0;
  float BareSlopeBandDeg = 0.0f;

  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
  wgpu::Buffer Uni, TileBuf;
  wgpu::Buffer ClassBuf;
  wgpu::RenderBundle Bundle;
  uint64_t BundleSig = 0;
  long TerrainBundleRecords = 0;

  int MaxLayers = 256;
  long ClassBytes = 0, MeshBytes = 0;
  double ClsEast[3] = {1, 0, 0}, ClsNorth[3] = {0, 1, 0}, ClsCam[2] = {0, 0};
  std::vector<DynTile> DynTiles;
  float LoggedSigma0 = -1.0f, LoggedSunThru = -1.0f;
  std::vector<int> DrawList;
  int DrawCalls = 0;

  long NotReadyDraws = 0, WrongModeDraws = 0, BlackDraws = 0;
  long DrawnVerts = 0;
  long DrawnByLevel[kLevelBins] = {};
  int VisibleTiles = 0;
};

} // namespace outshine::Render
#endif
