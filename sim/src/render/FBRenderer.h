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
#include "stages/FBTransmittanceStage.h"
#include "stages/FBSkyViewStage.h"
#include "stages/FBSkyStage.h"
#include "stages/FBTilesStage.h"

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
   * FBTerrainLoader, or synthesized — see FBAppNative.cpp). Call before Init/InitOffscreen (or before
   * the device is ready); FBTilesStage uploads it at Configure(). */
  void SetTerrain(const float *verts, uint32_t nverts, int ntiles, const uint32_t *voff,
                  const uint32_t *vcnt, const double *origins, const double *center);

  /* Real per-tile albedos: `layers` images of ts*ts RGBA8 (sRGB), layer i = tile i. Uploaded as a
   * texture_2d_array by FBTilesStage. Optional — without it, layers are procedural checkers. */
  void SetAlbedoArray(const uint8_t *rgba, int ts, int layers);

  /* ---- dynamic streaming (Stage 4): FBWorld drives a mutable tile table ----------------------
   * Enable before Init: the terrain source becomes per-tile GPU buffers + a growable albedo array
   * (layers recycled, grows to 2048), not the static SetTerrain mesh. */
  void SetStreaming(int albedoTS) { Tiles->EnableStreaming(albedoTS); }
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
  void ReleaseTile(int slot) { Tiles->ReleaseTile(slot); }         /* free the buffer, recycle BOTH albedo layers */
  void SetDrawList(const int *slots, int n) { Tiles->SetDrawList(slots, n); }  /* the tiles to draw this frame (FBWorld's leaves) */
  long AlbedoVramBytes(void) const { return Tiles->AlbedoVramBytes(); }  /* resident albedo VRAM (layers in use * ts^2 * 4) */

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
  int  DrawCount(void) const { return Tiles->DrawCount(); }   /* tile draws/frame (TileBuf = n*32 B) */

  /* Boot default = the EAGER base albedo (uploaded with each tile). The OTHER mode is the lazy overlay
   * (DynTile.PhotoLayer, fetched on first switch). 1 = EVS/photo base (Esri first), 0 = OSM base. */
  void SetDefaultMode(int photo) { Tiles->SetDefaultMode(photo); }

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

  void CreateTerrainPipeline(void);   /* DepthTex/HdrTex (shared scene targets) + Tiles->Configure() */
  void CreateTonemapPipeline(void);   /* fullscreen ACES tonemap: HDR -> sRGB swapchain */
  void CreatePresent(void);           /* fixed-720p frame target + the display upscale pass */
  void SyncSwapSize(void);            /* Surface mode: match the swapchain to canvas clientSize x DPR */
  void CreateTileTexture(void);       /* the shared linear sampler (terrain albedo + tonemap's HdrTex read) */
  void CreateAtmosphere(void);        /* Hillaire LUT/uniform/moon resources + Configure()s the 3 atmosphere stages */
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
  wgpu::RenderPipeline TonemapPipe, TonemapPlainPipe;   /* Plain = tonemap without the cloud composite */
  wgpu::BindGroup TonemapBind, TonemapBindH[2], TonemapBindPlain;   /* TonemapBindH[k] composites CloudHist[k]; Plain = no cloud */
  bool CloudsOn = false;   /* FB_CLOUDS=1 arms the whole volumetric+dome cloud path; default off (build the pass, skip it) */
  wgpu::Sampler Samp;              /* shared linear sampler: FBTilesStage's albedo, tonemap's HdrTex read */
  wgpu::Texture DepthTex, HdrTex;  /* shared scene targets: FBTilesStage/FBSkyStage draw into them, clouds sample DepthTex */
  std::unique_ptr<FBTilesStage> Tiles = std::make_unique<FBTilesStage>();

  /* Present path: the whole frame (scene + tonemap + HUD) lands in a FIXED 720p FrameTex; one upscale
   * pass then samples it onto the swapchain at the live display resolution (canvas clientSize x DPR). */
  wgpu::Texture FrameTex;
  std::unique_ptr<FBUpscaleStage> Upscale = std::make_unique<FBUpscaleStage>();
  int SwapW, SwapH;                   /* live swapchain (display) size; scene stays Width x Height */

  /* Hillaire-2020 atmosphere (Stage 5): two compute LUTs + a fullscreen sky pass; the transmittance
   * LUT also drives the terrain's aerial perspective. All linear radiance into the HDR chain. These
   * resources stay FBRenderer-owned (like FrameTex) because 3+ consumers read them — the three stage
   * classes below hold only the pipeline/bind group built from views injected at Configure(). */
  wgpu::Texture TransLUT, SkyLUT;                 /* 256x64, 192x108 rgba16float (storage + sampled) */
  wgpu::Sampler LutSamp;                          /* linear, U-repeat (azimuth wraps), V-clamp */
  wgpu::Buffer AtmoBuf;                           /* shared atmosphere uniform (sun, camera basis) */
  std::unique_ptr<FBTransmittanceStage> Transmittance = std::make_unique<FBTransmittanceStage>();
  std::unique_ptr<FBSkyViewStage> SkyView = std::make_unique<FBSkyViewStage>();
  std::unique_ptr<FBSkyStage> Sky = std::make_unique<FBSkyStage>();

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

  double Center[3];   /* terrain field centre — the default orbit camera's fallback target only */
  bool GroundPhoto;   /* SetGroundMode: the currently VIEWED mode (TAB); drives sun/moon/cloud selection too */
  int MaxLayers = 256;   /* device's real texture-array-layer cap (OnAdapter); handed to Tiles->Configure() */
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
