/* FlightBox — FBTilesStage: the terrain draw (kTerrainWGSL) — the one stage with real per-frame CPU
 * state, ported verbatim from FBRenderer: the growable albedo texture_2d_array (static bake OR
 * FBWorld's streamed per-tile table), the RenderBundle that bakes ~N per-tile draws once and replays
 * them (re-recorded only on STRUCTURE change), the 2-phase-commit tile table (a tile draws only one
 * pass after its GPU upload lands), and the SVS/EVS-strictness + black-tile invariant counters.
 *
 * Depends on FBTransmittanceStage's/FBSkyViewStage's LUTs (aerial perspective) and the shared AtmoBuf
 * — all three injected at Configure(), per the atmosphere Init-order contract documented in
 * FBRenderer::CreateAtmosphere (this stage's Configure() must run AFTER that). `samp` is FBRenderer's
 * shared linear sampler (also used by the tonemap pass reading HdrTex) — genuinely cross-stage, so it
 * stays FBRenderer-owned rather than duplicated here. */
#ifndef FBTILESSTAGE_H
#define FBTILESSTAGE_H

#include <cstdint>
#include <vector>
#include "FBDrawStage.h"

namespace FlightBox {

class FBTilesStage : public FBDrawStage {
public:
  void Configure(const FBGpu &gpu, wgpu::Sampler samp, wgpu::TextureView transLutView,
                wgpu::TextureView skyLutView, wgpu::Buffer atmoBuf, int maxLayers);

  /* Static terrain (SetTerrain path): upload the merged mesh + optional real per-tile bakes once. */
  void SetStaticMesh(const float *verts, uint32_t nverts, int ntiles, const uint32_t *voff,
                     const uint32_t *vcnt, const double *origins);
  void SetAlbedoArray(const uint8_t *rgba, int ts, int layers);

  /* Dynamic streaming (FBWorld drives a mutable per-tile GPU table + growable albedo array) instead
   * of the static SetStaticMesh source. Call before Configure(). */
  void EnableStreaming(int albedoTS) { Streaming = true; AlbedoTS = albedoTS; }
  void SetDefaultMode(int photo) { BaseMode = photo ? 1 : 0; }   /* boot default: eager base layer */

  /* Upload one streamed tile (mesh + its RGBA8-sRGB albedo). Returns a table slot id, or -1 if the
   * albedo array is full. Caller (FBRenderer) has already checked DeviceUsable(). */
  int UploadTile(const float *verts, uint32_t nverts, const double origin[3], const uint8_t *albedo,
                int ts, int z);
  /* Attach the aerial-photo albedo to an already-uploaded tile; `frameNo` timestamps the 2-phase
   * commit (the tile draws the photo layer only once its upload is a pass old). */
  int UploadTilePhoto(int slot, const uint8_t *photo, int ts, int z, unsigned frameNo);
  void ReleaseTile(int slot);
  void SetDrawList(const int *slots, int n) { DrawList.assign(slots, slots + n); }
  long AlbedoVramBytes(void) const { return (long)(LayerUsed - (int)FreeLayers.size()) * AlbedoTS * AlbedoTS * 4; }
  int DrawCount(void) const { return Streaming ? (int)DrawList.size() : NTiles; }
  bool IsStreaming(void) const { return Streaming; }

  /* Per-frame: rebuilds per-draw storage (camera-relative offsets + albedo layer/gain), writes it,
   * (re)records the RenderBundle on structure change, and draws — self-contained, no external gating
   * needed (there is always terrain to draw once configured). */
  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

  /* Invariant telemetry (FBRenderer's periodic [present] log reads these). */
  long GetNotReadyDraws(void) const { return NotReadyDraws; }
  long GetWrongModeDraws(void) const { return WrongModeDraws; }
  long GetBlackDraws(void) const { return BlackDraws; }
  long GetBundleRecords(void) const { return TerrainBundleRecords; }

private:
  struct DynTile { wgpu::Buffer Vtx; uint32_t NVerts; double Origin[3]; int Layer; int PhotoLayer;
                   unsigned PhotoUpTick; bool Used; };

  void EnsureAlbedoCap(int need);
  void RebuildBind(void);
  int AllocLayer(void);
  void WriteAlbedoLayer(int layer, const uint8_t *pyramid, int ts);
  void SetLayerPhoto(int layer, float ylin, int z);
  void ClearLayer(int layer);
  void UpdatePhotoGains(void);

  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::TextureFormat HdrFormat;
  wgpu::Sampler Samp;   /* borrowed from FBRenderer, shared with the (not yet extracted) tonemap pass */
  wgpu::TextureView TransLutView, SkyLutView;
  wgpu::Buffer AtmoBuf;

  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
  wgpu::Buffer Vtx, Uni, TileBuf;
  wgpu::Texture Albedo;
  wgpu::RenderBundle Bundle;
  uint64_t BundleSig = 0;
  long TerrainBundleRecords = 0;

  std::vector<float> TerrainVerts;
  std::vector<uint32_t> TileOff, TileCnt;
  std::vector<double> TileOrigin;
  std::vector<uint8_t> AlbedoData;
  uint32_t TerrainNVerts = 0;
  int NTiles = 0;
  int AlbedoTS = 0;

  bool Streaming = false;
  int BaseMode = 0;
  int LayerCap = 0, LayerUsed = 0, MaxLayers = 256;
  std::vector<DynTile> DynTiles;
  std::vector<int> FreeLayers;
  std::vector<float> Gains;
  std::vector<float> LayerYlin;
  std::vector<int8_t> LayerKind;
  double PhotoYTarget = 0.0;
  bool PhotoYValid = false;
  std::vector<int> DrawList;

  long NotReadyDraws = 0, WrongModeDraws = 0, BlackDraws = 0;
};

} // namespace FlightBox
#endif
