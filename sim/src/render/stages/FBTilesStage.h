/* The terrain draw — the ONE stage with real per-frame CPU state: the growable albedo array, the
 * RenderBundle, the 2-phase-commit tile table and the mode-strictness invariant counters.
 * Its Configure() must run AFTER FBRenderer::CreateAtmosphere, whose LUT views it is handed.
 * doc/flightbox/rendering.md, Abschnitt 6. */
#ifndef FBTILESSTAGE_H
#define FBTILESSTAGE_H

#include <cstdint>
#include <vector>
#include "FBDrawStage.h"

namespace FlightBox::Render {

class FBTilesStage : public FBDrawStage {
public:
  void Configure(const FBGpu &gpu, wgpu::Sampler samp, wgpu::TextureView transLutView,
                wgpu::TextureView skyLutView, wgpu::Buffer atmoBuf, int maxLayers);

  /* The static path: upload the merged mesh once. */
  void SetStaticMesh(const float *verts, uint32_t nverts, int ntiles, const uint32_t *voff,
                     const uint32_t *vcnt, const double *origins);
  void SetAlbedoArray(const uint8_t *rgba, int ts, int layers);

  /* FBWorld drives a mutable per-tile table instead. Call before Configure(). */
  void EnableStreaming(int albedoTS) { Streaming = true; AlbedoTS = albedoTS; }
  void SetDefaultMode(int photo) { BaseMode = photo ? 1 : 0; }   /* boot default: eager base layer */

  /* A table slot id, or -1 if the albedo array is full. The caller has checked DeviceUsable(). */
  int UploadTile(const float *verts, uint32_t nverts, const double origin[3], const uint8_t *albedo,
                int ts, int z);
  /* `frameNo` timestamps the 2-phase commit: the photo layer draws only once its upload is a pass old. */
  int UploadTilePhoto(int slot, const uint8_t *photo, int ts, int z, unsigned frameNo);
  void ReleaseTile(int slot);
  void SetDrawList(const int *slots, int n) { DrawList.assign(slots, slots + n); }
  long AlbedoVramBytes(void) const { return (long)(LayerUsed - (int)FreeLayers.size()) * AlbedoTS * AlbedoTS * 4; }
  int DrawCount(void) const { return Streaming ? (int)DrawList.size() : NTiles; }
  bool IsStreaming(void) const { return Streaming; }

  /* Self-contained: there is always terrain to draw once configured. */
  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

  /* Invariant telemetry, read by FBRenderer's periodic [present] log. */
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

} // namespace FlightBox::Render
#endif
