/* The WebGPU rendering system, one source and two link targets (emdawnwebgpu / native Dawn).
 * ORCHESTRATOR: it owns device/surface/targets and EVERY Begin/EndRenderPass boundary plus the encode
 * order; drawing lives in FBDrawStage-derived classes that record into the encoder it already opened.
 * A stage never begins or ends a pass. Pass-Topologie als Vertrag: doc/render/renderer.md §2. */
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
#include "stages/FBMapSheetStage.h"
#include "stages/FBGroundMapStage.h"
#include "stages/FBNvisStage.h"
#include "stages/FBUpscaleStage.h"
#include "stages/FBTransmittanceStage.h"
#include "stages/FBSkyViewStage.h"
#include "stages/FBSkyStage.h"
#include "stages/FBSunStage.h"
#include "stages/FBMoonStage.h"
#include "stages/FBTilesStage.h"
#include "stages/FBCloudLayerStage.h"
#include "stages/FBTonemapStage.h"

namespace FlightBox::Render {

class FBRenderer {
public:
  FBRenderer();

  /* Async bring-up (WASM): poll Ready() from the app loop. */
  void Init(const char *canvasSelector, int width, int height);

  /* Native: the same chain, blocking (no browser event loop to pump), into an offscreen target. */
  void InitOffscreen(int width, int height);

  bool Ready(void) const { return DeviceReady; }

  /* Acquire the target, run the passes, submit. */
  void RenderFrame(void);

  /* Offscreen only: tightly packed W*H*4 RGBA8, already sRGB-encoded — ready for stb_image_write. */
  bool ReadPixels(std::vector<uint8_t> &rgba);

  /* The STATIC terrain path. Call before Init; FBTilesStage uploads it at Configure(). */
  void SetTerrain(const float *verts, uint32_t nverts, int ntiles, const uint32_t *voff,
                  const uint32_t *vcnt, const double *origins, const double *center);

  /* Optional: without it the layers are procedural checkers. */
  void SetAlbedoArray(const uint8_t *rgba, int ts, int layers);

  /* Enable before Init: the terrain source becomes per-tile buffers + a growable albedo array
   * driven by FBWorld, not the static SetTerrain mesh. */
  void SetStreaming(int albedoTS) { Tiles->EnableStreaming(albedoTS); }
  bool DeviceUsable(void) const { return DeviceReady && !DeviceLost; }

  /* A table slot id, or -1 if the device is gone or the albedo array is full. */
  int  UploadTile(const float *verts, uint32_t nverts, const double origin[3], const uint8_t *albedo,
                  int ts, int z);
  /* The lazy overlay: a SECOND array layer for `slot`. 0 = the caller stops retrying and the tile
   * keeps drawing its base. */
  int  UploadTilePhoto(int slot, const uint8_t *photo, int ts, int z);
  void ReleaseTile(int slot) { Tiles->ReleaseTile(slot); }         /* free the buffer, recycle BOTH albedo layers */
  void SetDrawList(const int *slots, int n) { Tiles->SetDrawList(slots, n); }  /* the tiles to draw this frame (FBWorld's leaves) */
  long AlbedoVramBytes(void) const { return Tiles->AlbedoVramBytes(); }  /* resident albedo VRAM (layers in use * ts^2 * 4) */

  /* The live pose the symbology reads; `have` false -> "NO TELEMETRY". */
  void SetHud(const FBState &s, bool have);
  void SetHudEnabled(bool e) { HudEnabled = e; }   /* draw the HUD overlay (off for the cloud lab) */
  /* Borrowed from the active module and forwarded to FBHudStage; nullptr draws an empty HUD. */
  void SetHudDisplay(const Systems::FBDisplaySystem *disp) { Hud->SetDisplaySystem(disp); }
  /* THE TACTICAL MAP as the frame's overlay: it REPLACES the cockpit symbology in the same pass, so
   * switching views costs no Begin*Pass and the per-frame pass count is unchanged. Borrowed. */
  void SetMapOverlay(const Systems::FBHudGeometry *g) { Hud->SetOverlay(g); }
  /* THE MAP'S GROUND: an OSM raster sheet under the symbology, drawn first inside the SAME HUD pass.
   * The byte source is the client's (render/ owns no tile client); off = the cockpit frame. */
  void SetMapSheetSource(FBMapSheetStage::FBTileFetch f) { MapSheet->SetSource(f); }
  void SetMapSheet(const FBMapView &v, bool on) { MapSheet->SetView(v, on); }
  void PumpMapSheet(void) { MapSheet->Pump(); }
  FBMapSheetState MapSheetState(void) const { return MapSheet->State(); }
  int MapSheetZoom(void) const { return MapSheet->Zoom(); }
  int MapSheetResident(void) const { return MapSheet->TilesResident(); }
  int MapSheetWanted(void) const { return MapSheet->TilesWanted(); }
  /* While on, RenderFrame draws only the loading text and the client keeps JSBSim frozen. §2.2 */
  void SetLoadingScreen(bool on, float pct, int ready, int total) { LoadingScreen = on; LoadPct = pct; LoadReady = ready; LoadTotal = total; }

  /* The caller owns the DEM lookup. Negative = below terrain, never hidden. */
  void SetAgl(float agl);

  /* 0 = OSM/SVS (a constant-daylight database view), 1 = photo/EVS (lit by the real ephemeris sun).
   * Per tile, with a silent fallback, so the flip is never a hole. */
  void SetGroundMode(int photo);
  int  GetGroundMode(void) const { return GroundPhoto ? 1 : 0; }
  int  DrawCount(void) const { return Tiles->DrawCount(); }   /* tile draws/frame (TileBuf = n*32 B) */

  /* The EAGER base albedo uploaded with each tile; the other mode becomes the lazy overlay. */
  void SetDefaultMode(int photo) { Tiles->SetDefaultMode(photo); }

  /* Call before Init; without it the moon disc falls back to flat grey. */
  void SetMoonTexture(const uint8_t *rgba, int w, int h);

  /* Multiplier over the true angular radius (1 = realistic ~0.5 deg, ~5 px at 720p). */
  void SetMoonScale(double s) { MoonScale = s > 0.0 ? s : 1.0; }

  /* Scales the march step counts; 0 disables the cloud pass entirely. */
  void SetCloudQuality(double q) { Clouds->SetQuality(q); }

  /* The weather sample the clouds are built from, sampled by the CLIENT (which knows where it is) via
   * core/FBCloudDensity.h's FBCloudSkyFromWeather. Unset = no decks = no cloud pass at all; the
   * TERRAIN gets the same sample (haze + how much sun a deck lets down to it). */
  void SetCloudSky(const FBCloudSky &sky);

  /* 6 B/star, mag-sorted. Call before Init; placed at true alt/az for the given origin. */
  void SetStars(const uint8_t *hyg, int nbytes, double originLat, double originLon);

  /* Drives sidereal star placement. */
  void SetSkyClock(double unixSec) { SkyClock = unixSec; }

  /* count * 7 floats [posRelAnchor.xyz, worldRadiusM, colorPremul.rgb]. The pass subtracts
   * (eye - anchor) per frame, so it stays camera-relative without a re-upload. */
  void SetLightAnchor(const double anchor[3]) { TileLights->SetAnchor(anchor); }
  void SetLights(const float *inst, int count) { TileLights->SetLights(inst, count); }

  /* Up is derived radial: no roll. */
  void SetCamera(const double eye[3], const double target[3]);

  /* Carries ROLL, so the horizon tilts at bank. Takes precedence over SetCamera/orbit. */
  void SetCameraBasis(const double eye[3], const double fwd[3], const double right[3],
                      const double up[3]);

  /* THE 3x3 GRID. The windscreen is the top two rows; the bottom row is the MFD bank, drawn by the HUD
   * stage into the HUD pass. It exists exactly when the cockpit does — the cloud lab runs with the HUD
   * off and gets the full frame it always had. doc/render/renderer.md §2.4. */
  int SceneW(void) const { return Width; }
  int SceneH(void) const { return Height; }
  int ViewH(void) const { return HudEnabled ? Height - Height / 3 : Height; }
  /* The boresight's NDC offset: the scene covers the whole frame (so the bank has a world to be
   * translucent over) while its centre sits at the WINDSCREEN's centre. 0 without a cockpit. */
  float ViewShiftNdc(void) const { return 1.0f - (float)ViewH() / (float)Height; }

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
  void CreateClouds(void);            /* Configure()s the one cloud stage (needs the LUTs + DepthTex) */

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
  std::unique_ptr<FBTonemapStage> Tonemap = std::make_unique<FBTonemapStage>();
  bool CloudsOn = true;   /* FB_CLOUDS=0 disarms the cloud pass at boot; ON costs nothing without weather */
  wgpu::Sampler Samp;              /* shared linear sampler: FBTilesStage's albedo, tonemap's HdrTex read */
  wgpu::Texture DepthTex, HdrTex;  /* shared scene targets: FBTilesStage/FBSkyStage draw into them, clouds sample DepthTex */
  std::unique_ptr<FBTilesStage> Tiles = std::make_unique<FBTilesStage>();

  /* The whole frame lands in a FIXED 720p FrameTex; only the upscale pass follows the display size. */
  wgpu::Texture FrameTex;
  std::unique_ptr<FBUpscaleStage> Upscale = std::make_unique<FBUpscaleStage>();
  int SwapW, SwapH;                   /* live swapchain (display) size; scene stays Width x Height */

  /* Hillaire 2020. These resources stay FBRenderer-owned because 3+ consumers read them; the stages
   * hold only the pipeline/bind group built from views injected at Configure(). §4 */
  wgpu::Texture TransLUT, SkyLUT;                 /* 256x64, 192x108 rgba16float (storage + sampled) */
  wgpu::Sampler LutSamp;                          /* linear, U-repeat (azimuth wraps), V-clamp */
  wgpu::Buffer AtmoBuf;                           /* shared atmosphere uniform (sun, camera basis) */
  std::unique_ptr<FBTransmittanceStage> Transmittance = std::make_unique<FBTransmittanceStage>();
  std::unique_ptr<FBSkyViewStage> SkyView = std::make_unique<FBSkyViewStage>();
  std::unique_ptr<FBSkyStage> Sky = std::make_unique<FBSkyStage>();

  /* Additive draws, encoded directly after Sky in the scene pass. FBMoonStage owns the albedo
   * texture; these are only the raw bytes staged until Moon->Configure(). */
  std::unique_ptr<FBSunStage> Sun = std::make_unique<FBSunStage>();
  std::unique_ptr<FBMoonStage> Moon = std::make_unique<FBMoonStage>();
  std::vector<uint8_t> MoonData;
  int MoonW, MoonH;
  double MoonScale;                               /* FB_MOON_SCALE (default 1 = true angular size) */
  double SkyClock;
  std::unique_ptr<FBStarsStage> Stars = std::make_unique<FBStarsStage>();

  /* Streamed and placed by FBWorld; drawn after the terrain, depth-tested for occlusion. */
  std::unique_ptr<FBTileLightsStage> TileLights = std::make_unique<FBTileLightsStage>();

  /* NoOp, but wired into the encode ORDER: Units right after terrain, Sprites right before the HUD. */
  std::unique_ptr<FBUnitsStage> Units = std::make_unique<FBUnitsStage>();
  std::unique_ptr<FBSpritesStage> Sprites = std::make_unique<FBSpritesStage>();

  /* THE cloud chain: one stage, one pass, straight into HdrTex. doc/render/clouds.md. */
  std::unique_ptr<FBCloudLayerStage> Clouds = std::make_unique<FBCloudLayerStage>();

  /* FBHudStage is the WebGPU backend only; FBRenderer keeps the pose itself because sun/moon/cloud
   * drive its own lighting math, not just the HUD. */
  std::unique_ptr<FBHudStage> Hud = std::make_unique<FBHudStage>();
  /* Same pass as the HUD, encoded before it: the sheet is ground, the symbology is over it. */
  std::unique_ptr<FBMapSheetStage> MapSheet = std::make_unique<FBMapSheetStage>();
  /* THE TWO SENSOR PICTURES A COCKPIT BAY CAN CARRY, same pass and same argument as the sheet above:
   * a page's video is drawn first and its symbology lands on top. Which bay each one gets is read off
   * FBMfdBlock in RenderFrame — the display and the stage may not disagree about that. */
  std::unique_ptr<FBGroundMapStage> GroundMap = std::make_unique<FBGroundMapStage>();
  std::unique_ptr<FBNvisStage> Nvis = std::make_unique<FBNvisStage>();
  /* Value-initialised at the DECLARATION, not just in the one constructor: RenderFrame reads the
   * weather fields whether or not SetHud was ever called, and a second constructor would silently drop
   * that guarantee. The zeros are a DEFINED state — "no weather report", for which the cloud march has
   * its own default deck. */
  FBState HudState{};
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

} // namespace FlightBox::Render
#endif
