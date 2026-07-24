/* FlightBox — FBRenderer: the WebGPU rendering system (Dawn header family; emdawnwebgpu on the web,
 * native Dawn for the CLI — write once, link twice). Orchestrator: owns instance/adapter/device/
 * surface/targets and EVERY Begin/EndRenderPass boundary (the pass topology + encode order); DRAWING
 * itself is delegated to FBDrawStage-derived classes (render/stages/) it holds and cycles in a fixed
 * order — a stage never begins/ends a pass, it records into the encoder FBRenderer already opened.
 * Bring-up stage: device init + a field of REAL fb-tiles terrain (osmmesh ECEF meshes, camera-
 * relative), textured with real per-tile OSM albedos (a texture_2d_array), lit under a physically-
 * based Hillaire-2020 sky (compute LUTs + fullscreen sky pass + aerial perspective), drawn into an HDR
 * target, then an ACES tonemap pass to the swapchain ([0,1] reversed-Z depth on the scene pass), and a
 * MIL-STD-1787 HUD overlay pass on top (FBHudStage; reuses the WebGL engine's w3_build_hud symbology
 * via FBHud.h). */
#ifndef FBRENDERER_H
#define FBRENDERER_H

#include <cstdint>
#include <memory>
#include <vector>
#include <webgpu/webgpu_cpp.h>
#include "FBState.h"
#include "FBGpu.h"
#include "FBFrameContext.h"
#include "stages/FBStarsStage.h"
#include "stages/FBTileLightsStage.h"
#include "stages/FBUnitsStage.h"
#include "stages/FBSpritesStage.h"
#include "stages/FBHudStage.h"
#include "stages/FBUpscaleStage.h"

namespace FlightBox {

class FBRenderer {
public:
  FBRenderer();

  /* Async bring-up (WASM): instance -> adapter -> device -> surface(canvas selector) -> configured.
   * Ready() flips true when the chain completes; poll it from the app loop. */
  void Init(const char *canvasSelector, int width, int height);

  /* Synchronous bring-up (native CLI): same chain, blocking via Instance::WaitAny (native Dawn
   * supports this; no browser event loop to pump). Renders into an offscreen RGBA8-sRGB target
   * instead of a surface -> Ready() is final by the time this returns. */
  void InitOffscreen(int width, int height);

  bool Ready(void) const { return DeviceReady; }

  /* One frame: acquire the target (surface or offscreen texture), run the passes, submit. */
  void RenderFrame(void);

  /* Offscreen mode only: read the last-rendered frame back as tightly packed W*H*4 RGBA8 (already
   * sRGB-encoded by the tonemap pass) -> ready for stb_image_write. */
  bool ReadPixels(std::vector<uint8_t> &rgba);
  void ShapeStats(float cover, float low, float high);   /* numeric histogram of the base shape/density field over the shell */

  /* Hand the renderer a merged camera-relative ECEF terrain mesh + per-tile double origins (from
   * FBTerrainLoader, or synthesized — see FBAppNative.cpp). Call before Init/InitOffscreen (or before the
   * device is ready); CreateTerrainPipeline uploads it. */
  void SetTerrain(const float *verts, uint32_t nverts, int ntiles, const uint32_t *voff,
                  const uint32_t *vcnt, const double *origins, const double *center);

  /* Real per-tile albedos: `layers` images of ts*ts RGBA8 (sRGB), layer i = tile i. Uploaded as a
   * texture_2d_array in CreateTileTexture. Optional — without it, layers are procedural checkers. */
  void SetAlbedoArray(const uint8_t *rgba, int ts, int layers);

  /* ---- dynamic streaming (Stage 4): FBWorld drives a mutable tile table ----------------------
   * Enable before Init: the terrain source becomes per-tile GPU buffers + a growable albedo array
   * (layers recycled, grows to 2048), not the static SetTerrain mesh. */
  void SetStreaming(int albedoTS);
  bool DeviceUsable(void) const { return DeviceReady && !DeviceLost; }

  /* Upload one streamed tile (mesh + its RGBA8-sRGB albedo). Returns a table slot id, or -1 if the
   * device is gone or the albedo array is full (2048). Camera-relative ECEF: `origin` is subtracted
   * from the eye each frame. */
  int  UploadTile(const float *verts, uint32_t nverts, const double origin[3], const uint8_t *albedo,
                  int ts, int z);
  /* Attach the aerial-photo albedo to an already-uploaded tile (lazy, first TAB into EVS): allocates
   * a SECOND array layer for `slot`. 1 = attached (or already had one), 0 = device gone / array full
   * (caller stops retrying -> the tile keeps drawing OSM). */
  int  UploadTilePhoto(int slot, const uint8_t *photo, int ts, int z);
  void ReleaseTile(int slot);                 /* free the buffer, recycle BOTH albedo layers */
  void SetDrawList(const int *slots, int n);  /* the tiles to draw this frame (FBWorld's leaves) */
  long AlbedoVramBytes(void) const;           /* resident albedo VRAM (layers in use * ts^2 * 4) */

  /* MIL-STD-1787 HUD overlay: the live pose the symbology reads. `have` false -> "NO TELEMETRY".
   * Call each frame; without it the HUD pass is skipped. */
  void SetHud(const FBState &s, bool have);
  void SetHudEnabled(bool e) { HudEnabled = e; }   /* draw the HUD overlay (off for the cloud lab) */
  /* Boot/teleport loading screen: while on, RenderFrame draws a black screen with a MAX7456-style
   * "LOADING TERRAIN x%" + tile counter (no scene/sky), and the app keeps JSBSim frozen. Off -> normal. */
  void SetLoadingScreen(bool on, float pct, int ready, int total) { LoadingScreen = on; LoadPct = pct; LoadReady = ready; LoadTotal = total; }

  /* True AGL (m, ASL - DEM ground under the aircraft) for the HUD altitude tape + horizon dip. The
   * caller owns the DEM lookup (fb_stream_ground); forwarded to FBHudStage (w3_agl is its FBHud.h
   * global, only reachable from there now). Negative = below terrain, never hidden. */
  void SetAgl(float agl);

  /* Ground albedo source (TAB): 0 = OSM render (SVS, a constant-daylight database view), 1 = aerial
   * photo (EVS, lit by the real ephemeris sun the app feeds via SetHud). Per-tile: a tile without its
   * photo layer yet draws OSM (silent fallback), so the flip is never a hole. */
  void SetGroundMode(int photo);
  int  GetGroundMode(void) const { return GroundPhoto ? 1 : 0; }
  int  DrawCount(void) const { return Streaming ? (int)DrawList.size() : NTiles; }   /* tile draws/frame (TileBuf = n*32 B) */

  /* Boot default = the EAGER base albedo (uploaded with each tile). The OTHER mode is the lazy overlay
   * (DynTile.PhotoLayer, fetched on first switch). 1 = EVS/photo base (Esri first), 0 = OSM base. */
  void SetDefaultMode(int photo) { BaseMode = photo ? 1 : 0; }

  /* NASA LROC equirect moon albedo (RGBA8, w x h). Call before Init; the sky pass lights it as a
   * sphere by the real sun. Without it the moon disc falls back to a flat grey. */
  void SetMoonTexture(const uint8_t *rgba, int w, int h);

  /* Moon apparent-size multiplier over its true angular radius (FB_MOON_SCALE; default 1 = realistic
   * ~0.5deg, ~5 px at 720p). A tuning knob only. */
  void SetMoonScale(double s) { MoonScale = s > 0.0 ? s : 1.0; }

  /* Volumetric cloud raymarch quality (FB_CLOUD_QUALITY): scales step counts. 1 = best (default),
   * lower trades detail for FPS. 0 disables the cloud pass. */
  void SetCloudQuality(double q) { CloudQuality = q; }

  /* Cloud-lab override: force the material params (coverage, density, extinction, sun intensity, detail
   * strength) for the parameter-sweep rig, bypassing the weather-derived values. */
  void SetCloudLab(float cover, float density, float extinct, float sunI, float detail) {
    CloudLab = true; LabCover = cover; LabDensity = density; LabExtinct = extinct; LabSunI = sunI; LabDetail = detail;
  }
  /* Discard the temporal history (lab: reset accumulation before each parameter cell). */
  void ResetCloudHistory(void) { HistValid = false; AccumN = 0; }
  void SetAccumMode(bool on) { AccumMode = on; }   /* lab proof: true 1/N average instead of exponential blend */

  /* HYG star catalogue bytes (6 B/star, mag-sorted; the /t/stars bands concatenated). Call before
   * Init; the renderer decodes + places them at true alt/az for the given origin. */
  void SetStars(const uint8_t *hyg, int nbytes, double originLat, double originLon);

  /* Sim UTC (Unix seconds) driving sidereal star placement — the app sets it each frame (or once). */
  void SetSkyClock(double unixSec) { SkyClock = unixSec; }

  /* Night-light instances (EVS night): FBWorld streams /t/lights, decodes to camera-anchor-relative
   * ECEF, and uploads here. `inst` = count * 7 floats [posRelAnchor.xyz, worldRadiusM, colorPremul.rgb].
   * SetLightAnchor gives the ECEF the positions are relative to (the world origin) — set once. The pass
   * subtracts (eye - anchor) per frame, so it stays camera-relative without a per-frame re-upload. */
  void SetLightAnchor(const double anchor[3]) { TileLights->SetAnchor(anchor); }
  void SetLights(const float *inst, int count) { TileLights->SetLights(inst, count); }

  /* Scripted camera: eye + look-at target in ECEF (double); up is derived radial (no roll). */
  void SetCamera(const double eye[3], const double target[3]);

  /* Full ECEF camera basis (eye + forward/right/up), e.g. from an aircraft attitude — carries ROLL,
   * so the horizon tilts at bank. Takes precedence over SetCamera/orbit. */
  void SetCameraBasis(const double eye[3], const double fwd[3], const double right[3],
                      const double up[3]);

  const wgpu::Device &GetDevice(void) const { return Device; }
  wgpu::TextureFormat GetSurfaceFormat(void) const { return SurfaceFormat; }

private:
  enum class Target { Surface, Offscreen };

  void CreateTerrainPipeline(void);   /* scene: per-tile draws, [0,1] reversed-Z, textured, HDR out */
  void CreateTonemapPipeline(void);   /* fullscreen ACES tonemap: HDR -> sRGB swapchain */
  void CreatePresent(void);           /* fixed-720p frame target + the display upscale pass */
  void SyncSwapSize(void);            /* Surface mode: match the swapchain to canvas clientSize x DPR */
  void CreateTileTexture(void);       /* per-tile albedo texture_2d_array (real bake or procedural) */
  void EnsureAlbedoCap(int need);     /* grow the albedo array (recreate+copy) up to 2048 layers */
  int  AllocLayer(void);              /* one free albedo-array layer (recycled or grown), -1 if full */
  void WriteAlbedoLayer(int layer, const uint8_t *rgba, int ts);   /* upload one ts^2 sRGB image */
  void SetLayerPhoto(int layer, float ylin, int z);   /* record a photo layer's mean-Y + near/far kind */
  void ClearLayer(int layer);                          /* on release: drop the layer's photo bookkeeping */
  void UpdatePhotoGains(void);         /* EMA the near-tile mean into Ytarget, recompute far-tile gains */
  void RebuildTerrainBind(void);      /* (re)make the terrain bind group after an albedo-array swap */
  void CreateAtmosphere(void);        /* Hillaire LUTs (compute) + the sky render pipeline (+ moon) */
  void UpdateAtmosphere(const double eye[3], const double sunDir[3], const double right[3],
                        const double camUp[3], const double fwd[3], const double moonDir[3],
                        double dayF, double moonPh, double cloud);    /* per-frame atmosphere uniform */
  void CreateClouds(void);            /* Perlin-Worley 3D noise (compute) + the volumetric raymarch pass */
  void UpdateClouds(const double eye[3], const double sunDir[3], const double up[3], double nowSec);

  void StartAdapterRequest(void);
  void OnAdapter(wgpu::Adapter a);
  void OnDevice(wgpu::Device d);
  void ConfigureSurface(void);
  void CreateOffscreenTarget(void);

  wgpu::Instance Instance;
  wgpu::Adapter Adapter;
  wgpu::Device Device;
  wgpu::Queue Queue;   /* cached once — per-frame GetQueue() churns wrapper refs (measured: device died one ref per frame) */
  wgpu::Surface Surface;
  wgpu::Texture OffscreenTex;   /* Target::Offscreen final color target: RGBA8UnormSrgb, RenderAttachment|CopySrc */
  wgpu::TextureFormat SurfaceFormat;
  wgpu::TextureFormat HdrFormat;   /* offscreen scene target: rg11b10ufloat where renderable, else rgba16float */
  wgpu::RenderPipeline TerrainPipe, TonemapPipe, TonemapPlainPipe;   /* Plain = tonemap without the cloud composite */
  wgpu::Buffer Vtx, Uni, TileBuf;
  wgpu::BindGroup Bind, TonemapBind, TonemapBindH[2], TonemapBindPlain;   /* TonemapBindH[k] composites CloudHist[k]; Plain = no cloud */
  /* Terrain draws (~172/frame) recorded once into a RenderBundle, replayed per frame; re-recorded only
   * when the draw-list STRUCTURE changes (tile set / per-tile Vtx / NVerts / the Bind after an array
   * grow) — TileBuf/uniform CONTENTS stay per-frame buffer writes the bundle references by handle. */
  wgpu::RenderBundle TerrainBundle;
  uint64_t TerrainBundleSig = 0;     /* FNV of the structure; 0 = none recorded yet */
  long TerrainBundleRecords = 0;     /* re-record count (telemetry: ~few/s in a loiter, ~0 when parked) */
  bool CloudsOn = false;   /* FB_CLOUDS=1 arms the whole volumetric+dome cloud path; default off (build the pass, skip it) */
  wgpu::Sampler Samp;
  wgpu::Texture Albedo, DepthTex, HdrTex;

  /* Present path: the whole frame (scene + tonemap + HUD) lands in a FIXED 720p FrameTex; one upscale
   * pass then samples it onto the swapchain at the live display resolution (canvas clientSize x DPR). */
  wgpu::Texture FrameTex;
  std::unique_ptr<FBUpscaleStage> Upscale = std::make_unique<FBUpscaleStage>();
  int SwapW, SwapH;                   /* live swapchain (display) size; scene stays Width x Height */

  /* Hillaire-2020 atmosphere (Stage 5): two compute LUTs + a fullscreen sky pass; the transmittance
   * LUT also drives the terrain's aerial perspective. All linear radiance into the HDR chain. */
  wgpu::Texture TransLUT, SkyLUT;                 /* 256x64, 192x108 rgba16float (storage + sampled) */
  wgpu::ComputePipeline TransPipe, SkyLUTPipe;
  wgpu::RenderPipeline SkyPipe;
  wgpu::BindGroup TransBind, SkyLUTBind, SkyBind;
  wgpu::Sampler LutSamp;                          /* linear, U-repeat (azimuth wraps), V-clamp */
  wgpu::Buffer AtmoBuf;                           /* shared atmosphere uniform (sun, camera basis) */

  /* Real sky extras (EVS only): NASA moon albedo sampled as a lit sphere in the sky pass; a HYG star
   * field as an instanced additive quad pass. Both time-driven from the ephemeris the app feeds. */
  wgpu::Texture MoonTex;                          /* equirect RGBA8; 1x1 grey fallback if unset */
  std::vector<uint8_t> MoonData;
  int MoonW, MoonH;
  double MoonScale;                               /* FB_MOON_SCALE (default 1 = true angular size) */
  double SkyClock;
  std::unique_ptr<FBStarsStage> Stars = std::make_unique<FBStarsStage>();

  /* Night-light field (EVS night): instanced additive sprites at ground level, camera-anchor-relative
   * ECEF, class-coloured. Streamed + placed by FBWorld; drawn after terrain (depth-tested for occlusion). */
  std::unique_ptr<FBTileLightsStage> TileLights = std::make_unique<FBTileLightsStage>();

  /* Draw slots wired into the encode order but not yet real systems (see CLAUDE.md's units/ and the
   * F-16 display-system deferrals): Units draws right after terrain, Sprites right before the HUD pass. */
  std::unique_ptr<FBUnitsStage> Units = std::make_unique<FBUnitsStage>();
  std::unique_ptr<FBSpritesStage> Sprites = std::make_unique<FBSpritesStage>();

  /* Volumetric clouds (Nubis/MSFS-class): a Perlin-Worley 3D base + a Worley detail volume (compute-
   * generated once), raymarched through a WGS84 spherical shell into the HDR scene, depth-clipped by
   * the terrain. EVS-only; SVS leaves it off. */
  wgpu::Texture CloudBaseTex, CloudDetailTex;     /* 128^3 RGBA8 (perlin-worley + worley octaves), 32^3 RGB */
  wgpu::Texture CloudCellTex;                     /* 512² 2D F1-round cell field (B mode: vertical puffs) */
  wgpu::Texture CloudLowTex;                       /* QUARTER-RES march target (Width/4 x Height/4) rgba16float */
  wgpu::RenderPipeline CloudPipe;
  wgpu::BindGroup CloudBind;
  wgpu::Buffer CloudUni;
  wgpu::Sampler CloudSamp;                        /* 3D linear, repeat */
  int CloudW, CloudH;                             /* quarter-res dims */
  double CloudQuality;
  /* Temporal reprojection: a ping-pong history at quarter-res + an exponential-blend resolve pass.
   * The fresh jittered march accumulates into the history (reprojected by camera motion at the cloud
   * mid-shell), killing the per-frame "static" (05-temporal-reprojection.md). */
  wgpu::Texture CloudHist[2];                      /* rgba16float ping-pong (accumulated cloud) */
  wgpu::RenderPipeline CloudResolvePipe;
  wgpu::BindGroup CloudResolveBind[2];            /* [k] binds CloudHist[k] as the PREV history */
  wgpu::Buffer ResolveUni;
  float PrevVP[16] = {0};                          /* previous frame's MvpCamRel (reprojection) */
  double PrevEye[3] = {0, 0, 0};                   /* previous frame's ECEF eye (metres) */
  int HistCur = 0;
  bool HistValid = false;
  bool AccumMode = false;                          /* lab proof mode: weighted-splat running average */
  int AccumN = 0;                                  /* frames since reset */
  wgpu::Texture CloudWSum[2];                      /* accumulated splat weight per full-res pixel (r32float) */
  double CloudMidR = 6.362;                        /* cloud mid-shell radius Mm (reprojection depth) */
  /* GPU timestamp: brackets the cloud march + resolve; avg logged every 120 frames (real iGPU ms in WASM). */
  wgpu::QuerySet TsQuery;
  wgpu::Buffer TsResolveBuf, TsReadBuf;
  bool HasTimestamp = false, TsMapPending = false;
  double TsAccumMs = 0.0;
  int TsCount = 0;
  bool CloudLab = false;                          /* cloud-lab param override active */
  float LabCover = 0.5f, LabDensity = 5.0f, LabExtinct = 0.06f, LabSunI = 18.0f, LabDetail = 1.3f;

  /* HUD overlay (Stage 8): dynamic per-frame geometry, drawn after the tonemap. FBHudStage is the
   * WebGPU backend (MAX7456 atlas + solid/line/text pipelines); FBRenderer keeps the telemetry pose
   * itself too — sun/moon/cloud drive its OWN lighting math, not just the HUD. */
  std::unique_ptr<FBHudStage> Hud = std::make_unique<FBHudStage>();
  FBState HudState;
  bool HudEnabled, HudHave;
  bool LoadingScreen = false; float LoadPct = 0.0f; int LoadReady = 0, LoadTotal = 0;   /* boot loading screen */

  /* Terrain (set via SetTerrain, uploaded in CreateTerrainPipeline). Origins stay in DOUBLE: the
   * per-frame camera-relative offset origin-cam is computed on the CPU and streamed as float. */
  std::vector<float> TerrainVerts;      /* nverts*8 */
  std::vector<uint32_t> TileOff, TileCnt;
  std::vector<double> TileOrigin;       /* ntiles*3 */
  std::vector<uint8_t> AlbedoData;      /* NTiles layers of AlbedoTS^2 RGBA8 sRGB; empty -> procedural */
  double Center[3];
  uint32_t TerrainNVerts;
  int NTiles;
  int AlbedoTS;

  /* Dynamic streaming (Stage 4): a mutable per-tile GPU table + a growable albedo array. */
  /* Layer = the tile's eager BASE albedo (BaseMode); PhotoLayer = the OTHER mode's lazy OVERLAY. The
   * draw picks base when the viewed mode == BaseMode, else the committed overlay (fallback to base). */
  struct DynTile { wgpu::Buffer Vtx; uint32_t NVerts; double Origin[3]; int Layer; int PhotoLayer;
                   unsigned PhotoUpTick; bool Used; };
  long NotReadyDraws;                   /* 2-phase-commit assertion: draws of an uncommitted layer (must stay 0) */
  long WrongModeDraws = 0;              /* SVS<->EVS bleed: drew the other mode's layer (must stay 0) */
  long BlackDraws = 0;                  /* drew with no committed layer -> black tile (must stay 0) */
  bool Streaming;                       /* dynamic terrain source (FBWorld) instead of SetTerrain */
  bool GroundPhoto;                     /* SetGroundMode: the currently VIEWED mode (TAB) */
  int  BaseMode;                        /* boot default: which mode is the eager base layer (0 osm, 1 photo) */
  int LayerCap, LayerUsed, MaxLayers;   /* albedo array: allocated / high-water / device hard cap */
  std::vector<DynTile> DynTiles;        /* per-tile GPU buffers; free slots reused */
  std::vector<int> FreeLayers;          /* recycled albedo layer indices */
  std::vector<float> Gains;             /* per-albedo-layer photo brightness gain (1.0 = none) */
  std::vector<float> LayerYlin;         /* per-layer linear luminance of the tile MEAN (photo layers) */
  std::vector<int8_t> LayerKind;        /* 0 = none/OSM, 1 = far photo (gets gain), 2 = near photo (Ytarget ref) */
  double PhotoYTarget = 0.0;            /* adaptive brightness reference = EMA of resident near photo tiles */
  bool PhotoYValid = false;             /* false until a near photo tile has been seen (gain=1 fallback) */
  std::vector<int> DrawList;            /* slots to draw this frame */
  bool HaveCamera;                      /* SetCamera used (scripted path) vs the default orbit */
  bool CameraFull;                      /* SetCameraBasis used (full rolled basis) — wins over both */
  double Eye[3], LookTarget[3];
  double Fwd[3], Right[3], Up[3];       /* explicit ECEF camera basis (SetCameraBasis) */

  int Width, Height;
  bool DeviceReady, DeviceLost;
  Target Mode;
  bool Blocking;       /* native: RequestAdapter/RequestDevice via Instance::WaitAny, not callbacks */
  const char *Selector;
  unsigned FrameNo;
};

} // namespace FlightBox
#endif
