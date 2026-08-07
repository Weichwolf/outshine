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
  void Configure(const Gpu &gpu, wgpu::Sampler samp, wgpu::Sampler lutSamp,
                wgpu::TextureView skyLutView, wgpu::Buffer atmoBuf, int maxLayers,
                wgpu::Buffer vegTable, const SceneLight &light);

  /* The SAME weather sample the cloud pass marches (Renderer::SetCloudSky hands it to both): the
   * terrain reads its visibility for the haze and its decks for how much sun reaches the ground.
   * Unset = 100 km visibility, no deck — a clear standard atmosphere, not "no atmosphere". */
  void SetSky(const CloudSky &sky) { Sky = sky; }

  /* THE STAND THE GROUND FRAGMENT IS (render/Sward.h), placed on the world graticule so that what
   * grows at a point is a property of the point. `east/north/up` is the ECEF basis at the camera. */
  void SetSward(double lat, double lon, const double east[3], const double north[3],
                const double up[3]);

  /* World drives a mutable per-tile table. Call before Configure(). */
  void EnableStreaming(int albedoTS) { AlbedoTS = albedoTS; }

  /* A table slot id, or -1 if the class array is full. The caller has checked DeviceUsable().
   * `clusters` is the tile's DAG (doc/render/lod.md): every level lives in the one vertex buffer and
   * the per-frame cut picks the ranges. `cls` is ts*ts CLASS INDICES, one byte each — the raster
   * colour has already stopped existing on the CPU side. `anchor` is the ECEF point the procedural
   * surface is measured from; `origin - anchor` must stay small enough for float. */
  int UploadTile(const float *verts, uint32_t nverts, const DagCluster *clusters, int nclusters,
                const double origin[3], const double anchor[3], const uint8_t *cls, int ts);
  /* `frameNo` timestamps the 2-phase commit: the photo layer draws only once its upload is a pass old. */
  int UploadTilePhoto(int slot, const uint8_t *photo, int ts, int z, unsigned frameNo);
  void ReleaseTile(int slot);
  void SetDrawList(const int *slots, int n) { DrawList.assign(slots, slots + n); }
  /* The photo array, which is EMPTY until EVS is switched on for the first time. */
  long AlbedoVramBytes(void) const { return (long)(PhotoUsed - (int)FreePhoto.size()) * AlbedoTS * AlbedoTS * 4; }
  /* The classification input, one byte per texel. NO mip chain: the class is a DECISION per ground
   * point, so it may not be a function of the viewer's distance (doc/render/classification.md 0). */
  long ClassVramBytes(void) const {
    return (long)(LayerUsed - (int)FreeLayers.size()) * AlbedoTS * AlbedoTS;
  }
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
  long GetNotReadyDraws(void) const { return NotReadyDraws; }
  long GetWrongModeDraws(void) const { return WrongModeDraws; }
  long GetBlackDraws(void) const { return BlackDraws; }
  long GetBundleRecords(void) const { return TerrainBundleRecords; }

private:
  SceneLight Light;   /* borrowed: IrradianceStage's buffer + ShadowStage's cascades and atlas */

  /* Bound* is the whole tile over EVERY level, taken off the vertices and not off the DAG spheres. */
  struct DynTile { wgpu::Buffer Vtx; uint32_t NVerts; double Origin[3]; float Anchor[3]; int Layer;
                   int PhotoLayer; unsigned PhotoUpTick; bool Used; std::vector<DagCluster> Clusters;
                   float BoundCtr[3]; float BoundRad; };
  struct DrawRange { uint32_t Slot, First, Count; };

  static constexpr int kUniFloats = 44;   /* mat4 + sun + haze + the stand block; WGSL `U` verbatim */
  static constexpr int kTileFloats = 12;  /* the WGSL `Tile`: three vec4 */

  void EnsureClassCap(int need);
  void EnsurePhotoCap(int need);
  void RebuildBind(void);
  int AllocLayer(void);
  int AllocPhotoLayer(void);
  void WriteClassLayer(int layer, const uint8_t *cls, int ts);
  void WriteAlbedoLayer(int layer, const uint8_t *pyramid, int ts);
  void SetLayerPhoto(int layer, float ylin, int z);
  void ClearLayer(int layer);
  void UpdatePhotoGains(void);

  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::TextureFormat HdrFormat;
  wgpu::Sampler Samp;   /* borrowed from Renderer, shared with the (not yet extracted) tonemap pass */
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
  wgpu::Texture Albedo, Classes;
  wgpu::RenderBundle Bundle;
  uint64_t BundleSig = 0;
  long TerrainBundleRecords = 0;

  int AlbedoTS = 0;
  int LayerCap = 0, LayerUsed = 0, MaxLayers = 256;
  int PhotoCap = 0, PhotoUsed = 0;
  std::vector<DynTile> DynTiles;
  std::vector<int> FreeLayers, FreePhoto;
  std::vector<float> Gains;
  std::vector<float> LayerYlin;
  std::vector<int8_t> LayerKind;
  double PhotoYTarget = 0.0;
  bool PhotoYValid = false;
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
