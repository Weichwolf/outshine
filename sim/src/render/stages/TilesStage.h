/* The terrain draw — the ONE stage with real per-frame CPU state: the growable albedo array, the
 * RenderBundle, the 2-phase-commit tile table and the mode-strictness invariant counters.
 * Its Configure() must run AFTER Renderer::CreateAtmosphere, whose LUT views it is handed.
 * doc/render/renderer.md, Abschnitt 6. */
#ifndef TILESSTAGE_H
#define TILESSTAGE_H

#include <cstdint>
#include <vector>
#include "DrawStage.h"
#include "CloudDensity.h"
#include "ClusterDag.h"

namespace outshine::Render {

class TilesStage : public DrawStage {
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

  /* World drives a mutable per-tile table. Call before Configure(). */

  /* A table slot id, or -1 if the class array is full. The caller has checked DeviceUsable().
   * `clusters` is the tile's DAG (doc/render/lod.md): every level lives in the one vertex buffer and
   * the per-frame cut picks the ranges. `anchor` is the ECEF point the procedural surface is measured
   * from; `origin - anchor` must stay small enough for float. */
  int UploadTile(const float *verts, uint32_t nverts, const DagCluster *clusters, int nclusters,
                const double origin[3], const double anchor[3]);
  void ReleaseTile(int slot);
  void SetDrawList(const int *slots, int n) { DrawList.assign(slots, slots + n); }
  /* The classification input. It does not grow with the tile count, because it has nothing to do
   * with tiles — it is the vector geometry itself plus the grid that finds it. */
  long ClassVramBytes(void) const { return ClassBytes; }
  int DrawCount(void) const { return (int)DrawList.size(); }
  /* Draw CALLS the last Encode recorded, which is not the tile count once the cut merges runs and the
   * frustum drops tiles — the two numbers used to be equal and the budget needs the one that is paid. */
  int DrawCallCount(void) const { return (int)Ranges.size(); }
  /* Tiles the frustum kept, out of DrawCount(). */
  int VisibleTileCount(void) const { return VisibleTiles; }
  /* Triangles the LAST Encode actually submitted, not the ones resident — the budget curve
   * (doc/goal.md §5) is about what the frame paid for. Triangle-list, hence /3. */
  long TriangleCount(void) const { return DrawnVerts / 3; }

  /* Self-contained: there is always terrain to draw once configured. */
  void Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

  /* Invariant telemetry, read by Renderer's periodic [present] log. */
  /* The resident tiles as CASTERS: the same buffers the scene pass draws, never a second copy. A
   * shadow cascade is a different view, so it takes no part of the camera's cut — it takes the whole
   * residency and culls against its own box. */
  struct Caster { wgpu::Buffer Vtx; uint32_t NVerts; double Origin[3]; float BoundCtr[3], BoundRad; };
  void CollectCasters(std::vector<Caster> &out) const;

  long GetNotReadyDraws(void) const { return NotReadyDraws; }
  long GetWrongModeDraws(void) const { return WrongModeDraws; }
  long GetBlackDraws(void) const { return BlackDraws; }
  long GetBundleRecords(void) const { return TerrainBundleRecords; }

private:
  SceneLight Light;   /* borrowed: IrradianceStage's buffer + ShadowStage's cascades and atlas */

  /* Bound* is the whole tile over EVERY level, taken off the vertices and not off the DAG spheres. */
  struct DynTile { wgpu::Buffer Vtx; uint32_t NVerts; double Origin[3]; float Anchor[3];
                   bool Used; std::vector<DagCluster> Clusters;
                   float BoundCtr[3]; float BoundRad; };
  struct DrawRange { uint32_t Slot, First, Count; };

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

  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
  wgpu::Buffer Uni, TileBuf;
  wgpu::Buffer ClassBuf;
  wgpu::RenderBundle Bundle;
  uint64_t BundleSig = 0;
  long TerrainBundleRecords = 0;

  int MaxLayers = 256;
  long ClassBytes = 0;
  double ClsEast[3] = {1, 0, 0}, ClsNorth[3] = {0, 1, 0}, ClsCam[2] = {0, 0};
  std::vector<DynTile> DynTiles;
  float LoggedSigma0 = -1.0f, LoggedSunThru = -1.0f;
  std::vector<int> DrawList;
  std::vector<DrawRange> Ranges;   /* the cut, rebuilt every frame; reused, so no per-frame heap */
  long CutClusters = 0;

  long NotReadyDraws = 0, WrongModeDraws = 0, BlackDraws = 0;
  long DrawnVerts = 0;
  int VisibleTiles = 0;
};

} // namespace outshine::Render
#endif
