/* The WebGPU rendering system, one source and two link targets (emdawnwebgpu / native Dawn).
 * ORCHESTRATOR: it owns device/surface/targets and EVERY Begin/EndRenderPass boundary plus the encode
 * order; drawing lives in DrawStage-derived classes that record into the encoder it already opened.
 * A stage never begins or ends a pass. Pass-Topologie als Vertrag: doc/render/renderer.md §2. */
#ifndef RENDERER_H
#define RENDERER_H

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>
#include <webgpu/webgpu_cpp.h>
#include "State.h"
#include "Gpu.h"
#include "FrameContext.h"
#include "GpuTimer.h"
#include "OverlayStage.h"
#include "stages/StarsStage.h"
#include "stages/TileLightsStage.h"
#include "stages/UnitsStage.h"
#include "stages/SpritesStage.h"
#include "stages/UpscaleStage.h"
#include "stages/TransmittanceStage.h"
#include "stages/MultiScatterStage.h"
#include "stages/IrradianceStage.h"
#include "stages/SkyViewStage.h"
#include "stages/SkyStage.h"
#include "stages/SunStage.h"
#include "stages/MoonStage.h"
#include "stages/TilesStage.h"
#include "stages/BenchGroundStage.h"
#include "stages/BuildingsStage.h"
#include "stages/TreeStage.h"
#include "stages/CloudLayerStage.h"
#include "stages/ShadowStage.h"
#include "stages/AoStage.h"
#include "stages/ExposureStage.h"
#include "stages/TaaStage.h"
#include "stages/TonemapStage.h"
#include "TemporalJitter.h"

namespace outshine::Render {

class Renderer {
public:
  Renderer();

  /* Async bring-up (WASM): poll Ready() from the app loop. */
  void Init(const char *canvasSelector, int width, int height);

  /* Native: the same chain, blocking (no browser event loop to pump), into an offscreen target. */
  void InitOffscreen(int width, int height);

  bool Ready(void) const { return DeviceReady; }

  /* Acquire the target, run the passes, submit. */
  void RenderFrame(void);

  /* Offscreen only: tightly packed W*H*4 RGBA8, already sRGB-encoded — ready for stb_image_write. */
  bool ReadPixels(std::vector<uint8_t> &rgba);

  /* Blocks until everything submitted so far has retired. A per-frame wall clock without this reads
   * the encoder, not the frame. */
  void SyncGpu(void);

  /* Reversed-Z scene depth, W*H floats, row-major. Range along the view ray follows as
   * kNearM / depth / cos(angle off boresight); a critic's "at 1-2 km" is otherwise a guess about a
   * hillside's row. Blocking, offscreen only. */
  bool ReadDepth(std::vector<float> &depth);
  static constexpr float kNearM = 0.05f;   /* MvpCamRel's zn — the numerator of that division */

  /* [sunIrr.rgb, _, skyIrr.rgb, _] in top-of-atmosphere-solar = 1 units: the scale everything lit is
   * multiplied by, measurable instead of asserted. Blocking. */
  bool ReadIrradiance(float out[IrradianceStage::kFloats]);

  /* WHAT THE SCENE MAY DECLARE about its own brightness; Auto with no compensation is the default
   * and needs no declaration. Takes effect on the next frame. */
  void SetExposure(const ExposureParams &p) { Exposure->SetParams(p); }

  /* [outBlack, outWhite, contrast, adaptLog2, blackLog2, whiteLog2, horizE, _], all log2 of scene
   * radiance except horizE and the exponent. Blocking. */
  bool ReadExposure(float out[ExposureStage::kMeterFloats]);

  /* Enable before Init: the terrain source is per-tile buffers plus a growable CLASS array driven
   * by World. */
  bool DeviceUsable(void) const { return DeviceReady && !DeviceLost; }

  /* A table slot id, or -1 if the device is gone or the class array is full. */
  int  UploadTile(const float *verts, uint32_t nverts, const DagCluster *clusters, int nclusters,
                 const double origin[3], const double anchor[3]);
  /* THE CLASS STRUCTURE, borrowed for the length of the call (world/ClassField.h). */
  void SetClassFrame(const double east[3], const double north[3], const double camOffset[2]) {
    Tiles->SetClassFrame(east, north, camOffset);
  }
  void WriteClassBuffer(const uint32_t *words, size_t bytes) { Tiles->WriteClassBuffer(words, bytes); }
  void ReleaseTile(int slot) { Tiles->ReleaseTile(slot); }         /* free the buffer, recycle both layers */
  void SetDrawList(const int *slots, int n) { Tiles->SetDrawList(slots, n); }  /* the tiles to draw this frame (World's leaves) */
  long ClassVramBytes(void) const { return Tiles->ClassVramBytes(); }    /* the classification input */

  /* THE ENVIRONMENT the frame is lit and hazed by — sun, moon, cloud deck, altitude. Nothing to do
   * with a HUD any more: the renderer keeps it because sun/moon/cloud drive its own lighting math. */
  void SetSceneState(const State &s) { SceneState = s; }

  /* THE VEHICLE'S AVIONICS, borrowed and owned by whoever has the capability. nullptr (a pedestrian)
   * means no overlay pass and no avionics translation unit in the link at all. May be registered
   * before or after the device exists; Init() is driven from here either way. */
  void SetOverlay(OverlayStage *o);

  /* While on, RenderFrame draws only the loading text and the client keeps its sim frozen. §2.2 */
  void SetLoadingScreen(bool on, float pct, int ready, int total) { LoadingScreen = on; LoadPct = pct; LoadReady = ready; LoadTotal = total; }

  /* THE VEGETATION TABLE, before Init: 256 resolved rows (world/VegetationTemplates::Row). Without it
   * the terrain keeps its raw albedo — the pre-template picture, on purpose. */
  void SetVegetationTable(const void *rows, size_t rowBytes);

  /* WHERE THE STAND IS READ. The ground fragment IS the stand (render/Sward.h), so it needs the
   * place and the local basis and nothing else — the geodetic position picks the graticule cell the
   * canopy field is hashed on. */
  void SetSwardBasis(double lat, double lon, const double east[3], const double north[3],
                     const double up[3]) {
    Tiles->SetSward(lat, lon, east, north, up);
  }
  /* THE SUBJECT BENCH'S FLOOR AND ITS NEUTRAL CARD (doc/clients/clients.md, `gpu_walk --rig`) —
   * studio furniture, and off unless a bench declares it. `radiusM` <= 0 retires the floor and a card
   * of zero width retires the card, which is the state every other client is in for the whole of its
   * life. */
  void SetBenchGround(double eyeAglM, double radiusM, double gridM) {
    BenchGround->SetPlane(eyeAglM, radiusM, gridM);
  }
  void SetBenchSubstrate(const float linearRgb[3]) { BenchGround->SetSubstrate(linearRgb); }
  void SetBenchCard(const BenchCard &card) { BenchGround->SetCard(card); }

  /* THE SUBJECT BENCH'S PLANT, in the same camera-relative ground frame the bench's floor and card
   * use. The mesh arrives as raw arrays because render/ draws a tree it is handed and does not know
   * how a species grows; `heightM` <= 0 is the state every other client stays in. */
  void SetTreeBark(const float *verts, uint32_t nverts, const uint32_t *idx, uint32_t nidx) {
    Trees->SetBark(verts, nverts, idx, nidx);
  }
  void SetTreeLeaf(const float *verts, uint32_t nverts, const uint32_t *idx, uint32_t nidx,
                   const float *inst, uint32_t ninst, float scaleM) {
    Trees->SetLeaf(verts, nverts, idx, nidx, inst, ninst, scaleM);
  }
  void SetTreeLook(const TreeLook &look) { Trees->SetLook(look); }
  void SetTreeStand(double eastM, double northM, double eyeAglM, double heightM) {
    Trees->SetStand(eastM, northM, eyeAglM, heightM);
  }
  void SetTreeLeavesVisible(bool on) { Trees->SetLeavesVisible(on); }
  long TreeTriangleCount(void) const { return Trees->TriangleCount(); }

  /* THE SCENE'S DECLARED WIND, met convention (the bearing it comes from, m/s at 10 m). Nothing
   * below the size of a tree answers to it (doc/goal.md), so it is held for the consumers that owe a
   * published anchor — a branch and a rotor — and read by no stage today. */
  void SetWind(double fromDeg, double speedMs) { WindFromDeg = fromDeg; WindMs = speedMs; }
  /* The WIND clock, deliberately not the sky clock: a wave measurement wants the sun to stand still
   * while the field runs. Advanced by the client, never by the renderer. */
  void SetWindClock(double seconds) { WindClock = seconds; }
  double GetWindClock(void) const { return WindClock; }

  /* THE ACCUMULATION HAS NO CONTINUITY ACROSS A TELEPORT. A motion vector describes a step, not a
   * jump: after the camera is placed somewhere else outright, every reprojection points at a pixel
   * that shows something unrelated, and the neighbourhood clip is the only thing between that and a
   * ghost of the previous standpoint. A caller that MOVES the camera rather than walking it says so
   * here, and then renders TemporalSettleFrames() before it reads the picture. */
  void ResetTemporal(void) { HaveHistory = false; Jitter.Reset(); }
  /* MEASURED, not derived: the settled 1280x720 reference frame against the same frame after 512
   * settle frames, over the whole ladder (`--settle N`, 2026-08-07). Pixels differing by more than
   * two codes: 3924 at 0, 22 at 48, 7 at 128, and 7 at 192 / 256 / 384. 128 is where the curve
   * reaches its floor; below it the accumulator is still visibly filling, above it nothing moves.
   * The residual 7 px is the f16 history's last bit and does not go to zero at any length. */
  static constexpr int kTemporalSettleFrames = 128;
  int TemporalSettleFrames(void) const { return kTemporalSettleFrames; }

  /* The extruded OSM footprints World decoded; positions are ECEF offsets from `anchor`. */
  void SetBuildingMesh(const float *verts, uint32_t nverts, const DagCluster *clusters, int nclusters,
                       const double anchor[3]) {
    Buildings->SetMesh(verts, nverts, clusters, nclusters, anchor);
  }
  uint32_t BuildingVertexCount(void) const { return Buildings->VertexCount(); }

  int  DrawCount(void) const { return Tiles->DrawCount(); }   /* tile draws/frame (TileBuf = n*32 B) */
  int  TerrainDrawCalls(void) const { return Tiles->DrawCallCount(); }
  int  TerrainVisibleTiles(void) const { return Tiles->VisibleTileCount(); }
  long TerrainTriangleCount(void) const { return Tiles->TriangleCount(); }
  /* The two accumulation buffers plus the motion attachment — the whole price of TAA in memory. */
  long TemporalVramBytes(void) const {
    return Taa->HistoryBytes() + (long)Width * (long)Height * 4L;
  }
  long ShadowTriangleCount(void) const { return Shadow->TriangleCount(); }
  int  ShadowDrawCalls(void) const { return Shadow->DrawCallCount(); }

  /* THE BUDGET INSTRUMENT (doc/goal.md §5): triangles the last frame submitted, over every geometry
   * stage there is. */
  long TriangleCount(void) const {
    return Tiles->TriangleCount() + (long)Buildings->VertexCount() / 3 + Trees->TriangleCount();
  }

  /* Call before Init; without it the moon disc falls back to flat grey. */
  void SetMoonTexture(const uint8_t *rgba, int w, int h);

  /* Multiplier over the true angular radius (1 = realistic ~0.5 deg, ~5 px at 720p). */
  void SetMoonScale(double s) { MoonScale = s > 0.0 ? s : 1.0; }

  /* Scales the march step counts; 0 disables the cloud pass entirely. */
  void SetCloudQuality(double q) { CloudQuality = q; Clouds->SetQuality(q); }

  /* The weather sample the clouds are built from, sampled by the CLIENT (which knows where it is) via
   * core/CloudDensity.h's CloudSkyFromWeather. Unset = no decks = no cloud pass at all; the
   * TERRAIN gets the same sample (haze + how much sun a deck lets down to it). */
  void SetCloudSky(const CloudSky &sky);

  /* 6 B/star, mag-sorted. Call before Init; placed at true alt/az for the given origin. */
  void SetStars(const uint8_t *hyg, int nbytes, double originLat, double originLon);

  /* Drives sidereal star placement. */
  void SetSkyClock(double unixSec) { SkyClock = unixSec; }

  /* ---- Units ----
   * The MESH of an airframe is renderer data — vertices, materials, a baked texture and the sidecar's
   * part table — so unlike the terrain (a network service the client owns) it is read here, from a
   * directory the client names. Call before Init; the GPU upload happens with the pipeline.
   * `typeName` is the module registry key a unit publishes (`f16`). */
  bool AddUnitModel(const char *typeName, const char *dir) { return Units->AddModel(typeName, dir); }

  /* THE CAST FOR THIS FRAME, borrowed: World rebuilds it from the published poses every Update(),
   * and passing an empty list is what makes an empty world cost nothing. */
  void SetUnitDraws(const UnitDraw *draws, int count) { Units->SetDraws(draws, count); }

  /* THE EFFECTS FOR THIS FRAME, borrowed on the same terms as the cast above and built from the same
   * published data — flame, plume, flare, chaff. An empty list costs nothing. */
  void SetSpriteDraws(const SpriteDraw *s, int count) { Sprites->SetSprites(s, count); }

  /* The exhaust plane of a loaded type, model space, off the mesh — see UnitsStage::Nozzle. */
  bool UnitNozzle(const char *type, float off[3], float &radiusM) const {
    return Units->Nozzle(type, off, radiusM);
  }

  /* count * 7 floats [posRelAnchor.xyz, worldRadiusM, colorPremul.rgb]. The pass subtracts
   * (eye - anchor) per frame, so it stays camera-relative without a re-upload. */
  void SetLightAnchor(const double anchor[3]) { TileLights->SetAnchor(anchor); }
  void SetLights(const float *inst, int count) { TileLights->SetLights(inst, count); }

  /* Up is derived radial: no roll. */
  void SetCamera(const double eye[3], const double target[3]);

  /* Carries ROLL, so the horizon tilts at bank. Takes precedence over SetCamera/orbit. */
  void SetCameraBasis(const double eye[3], const double fwd[3], const double right[3],
                      const double up[3]);

  /* THE SCENE'S vertical field of view over the full frame height, as the scene file declares it.
   * Call before the first frame; it enters the projection and the atmosphere uniform together, so
   * there is never a second copy to drift from. */
  void SetFovDeg(double deg) { FovDeg = deg > 0.0 ? (float)deg : FovDeg; }
  float GetFovDeg(void) const { return FovDeg; }

  /* THE 3x3 GRID. The windscreen is the top two rows; the bottom row is an equipped body's MFD bank.
   * Without an overlay the scene keeps the whole frame. doc/render/renderer.md §2.4. */
  int SceneW(void) const { return Width; }
  int SceneH(void) const { return Height; }
  int ViewH(void) const { return Overlay ? Overlay->SceneViewH(Height) : Height; }
  /* The boresight's NDC offset: the scene covers the whole frame (so the bank has a world to be
   * translucent over) while its centre sits at the WINDSCREEN's centre. 0 without a cockpit. */
  float ViewShiftNdc(void) const { return 1.0f - (float)ViewH() / (float)Height; }

  const wgpu::Device &GetDevice(void) const { return Device; }
  wgpu::TextureFormat GetSurfaceFormat(void) const { return SurfaceFormat; }

private:
  enum class Target { Surface, Offscreen };

  void CreateTerrainPipeline(void);   /* DepthTex/HdrTex (shared scene targets) + Tiles->Configure() */
  void CreateTonemapPipeline(void);   /* the meter + the fullscreen display curve: HDR -> sRGB swapchain */
  void CreatePresent(void);           /* fixed-720p frame target + the display upscale pass */
  void SyncSwapSize(void);            /* Surface mode: match the swapchain to canvas clientSize x DPR */
  void CreateTileTexture(void);       /* the shared linear sampler (terrain albedo + tonemap's HdrTex read) */
  void CreateAtmosphere(void);        /* Hillaire LUT/uniform/moon resources + Configure()s the atmosphere stages */
  void CreateSceneLight(void);        /* shadow atlas + cascade uniform + irradiance buffer, before any lit stage */
  SceneLight Light(void) const;       /* the four handles a lit surface binds */
  void UpdateAtmosphere(const double eye[3], const double sunDir[3], const double right[3],
                        const double camUp[3], const double fwd[3], const double moonDir[3],
                        double dayF, double moonPh);    /* per-frame atmosphere uniform */
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
  wgpu::TextureFormat SurfaceFormat;   /* what pipelines and views use: always sRGB-encoding */
  wgpu::TextureFormat SwapFormat = wgpu::TextureFormat::BGRA8Unorm;   /* what the surface itself is */
  static wgpu::TextureFormat SrgbView(wgpu::TextureFormat f) {
    if (f == wgpu::TextureFormat::BGRA8Unorm) return wgpu::TextureFormat::BGRA8UnormSrgb;
    if (f == wgpu::TextureFormat::RGBA8Unorm) return wgpu::TextureFormat::RGBA8UnormSrgb;
    return f;
  }
  void ConfigureSwapchain(void);
  wgpu::TextureFormat HdrFormat;   /* offscreen scene target: rg11b10ufloat where renderable, else rgba16float */
  std::unique_ptr<TonemapStage> Tonemap = std::make_unique<TonemapStage>();
  /* Sun shadows and contact occlusion. Both own a target and neither owns a pass: the shadow atlas is
   * filled in a pass Renderer opens before the scene, the AO buffer in one it opens after. */
  std::unique_ptr<ShadowStage> Shadow = std::make_unique<ShadowStage>();
  std::unique_ptr<AoStage> Ao = std::make_unique<AoStage>();
  std::unique_ptr<ExposureStage> Exposure = std::make_unique<ExposureStage>();
  wgpu::Buffer CsmBuf;                            /* the cascade uniform every lit surface binds */
  wgpu::Sampler Samp;              /* shared linear sampler: unit textures, TAA history, tonemap's HdrTex read */
  wgpu::Texture DepthTex, HdrTex;  /* shared scene targets: TilesStage/SkyStage draw into them, clouds sample DepthTex */
  /* THE SECOND SCENE ATTACHMENT (stages/SceneTargets.h): screen-space motion of whatever owns the
   * depth. Cleared to the "world-fixed" sentinel every frame, written only by geometry that moved
   * for a reason other than the camera. */
  wgpu::Texture VelTex;
  /* THE TEMPORAL PAIR. Neither half is useful alone: the jitter without the resolve is a shaking
   * picture, the resolve without the jitter averages the same samples over and over. */
  std::unique_ptr<TaaStage> Taa = std::make_unique<TaaStage>();
  TemporalJitter Jitter;
  /* FB_TAA=0 — see RenderFrame. A measurement gate, not a setting: nothing in the tree turns the pair
   * off, and the one caller that may is a bench asking what the picture costs. */
  const bool TaaOn = [] { const char *e = getenv("FB_TAA"); return !e || atoi(e) != 0; }();
  bool HaveHistory = false;
  float PrevMvp[16] = {};
  double PrevEye[3] = {0, 0, 0};
  std::unique_ptr<TilesStage> Tiles = std::make_unique<TilesStage>();
  /* The 256 vegetation-template rows: the terrain shades its ground cover out of them. */
  wgpu::Buffer VegBuf;
  std::vector<uint8_t> VegRows;
  std::unique_ptr<BenchGroundStage> BenchGround = std::make_unique<BenchGroundStage>();
  std::unique_ptr<BuildingsStage> Buildings = std::make_unique<BuildingsStage>();
  std::unique_ptr<TreeStage> Trees = std::make_unique<TreeStage>();

  /* The whole frame lands in a FIXED 720p FrameTex; only the upscale pass follows the display size. */
  wgpu::Texture FrameTex;
  std::unique_ptr<UpscaleStage> Upscale = std::make_unique<UpscaleStage>();
  int SwapW, SwapH;                   /* live swapchain (display) size; scene stays Width x Height */

  /* Hillaire 2020. These resources stay Renderer-owned because 3+ consumers read them; the stages
   * hold only the pipeline/bind group built from views injected at Configure(). §4 */
  wgpu::Texture TransLUT, MsLUT, SkyLUT;          /* 256x64, 32x32, 192x108 rgba16float (storage + sampled) */
  wgpu::Sampler LutSamp;                          /* linear, U-repeat (azimuth wraps), V-clamp */
  GpuTimer GpuTime;                               /* per-pass GPU time, inert unless FB_GPUTIME */
  wgpu::Buffer AtmoBuf;                           /* shared atmosphere uniform (sun, camera basis) */
  wgpu::Buffer IrrBuf;                            /* IrradianceStage's two irradiances — THE scale */
  wgpu::Buffer MeterBuf;                          /* ExposureStage's gain + white point, read by the tonemap */
  std::unique_ptr<TransmittanceStage> Transmittance = std::make_unique<TransmittanceStage>();
  std::unique_ptr<MultiScatterStage> MultiScatter = std::make_unique<MultiScatterStage>();
  std::unique_ptr<SkyViewStage> SkyView = std::make_unique<SkyViewStage>();
  std::unique_ptr<IrradianceStage> Irradiance = std::make_unique<IrradianceStage>();
  std::unique_ptr<SkyStage> Sky = std::make_unique<SkyStage>();

  /* Additive draws, encoded directly after Sky in the scene pass. MoonStage owns the albedo
   * texture; these are only the raw bytes staged until Moon->Configure(). */
  std::unique_ptr<SunStage> Sun = std::make_unique<SunStage>();
  std::unique_ptr<MoonStage> Moon = std::make_unique<MoonStage>();
  std::vector<uint8_t> MoonData;
  int MoonW, MoonH;
  double MoonScale;                               /* FB_MOON_SCALE (default 1 = true angular size) */
  double SkyClock;
  double WindFromDeg = 0.0, WindMs = 0.0, WindClock = 0.0;
  std::unique_ptr<StarsStage> Stars = std::make_unique<StarsStage>();

  /* Streamed and placed by World; drawn after the terrain, depth-tested for occlusion. */
  std::unique_ptr<TileLightsStage> TileLights = std::make_unique<TileLightsStage>();

  /* Units right after terrain, Sprites right before the HUD — the effects last, because they are
   * additive/translucent and belong over everything solid in the scene. */
  std::unique_ptr<UnitsStage> Units = std::make_unique<UnitsStage>();
  std::unique_ptr<SpritesStage> Sprites = std::make_unique<SpritesStage>();

  /* THE cloud chain: one stage, one pass, straight into HdrTex. doc/render/clouds.md. */
  std::unique_ptr<CloudLayerStage> Clouds = std::make_unique<CloudLayerStage>();

  /* THE ONE cloud field, as a buffer: the march reads it and so does every lit surface, because a
   * shadow computed from a second field would not lie under its cloud. Renderer-owned for the same
   * reason AtmoBuf is — four consumers, one writer. */
  wgpu::Buffer CloudBuf;
  CloudSky SkyParams;
  double CloudQuality = 0.0;   /* 0 = the sheet; > 0 drives the volumetric march (stages/clouds.md) */
  void WriteCloudSky(const FrameContext &ctx);
  /* The field's horizontal origin is a PLACE (SkyParams.Anchor*), never the camera: pinning it at the
   * first eye made the cloud pattern a function of where the session started. */
  bool HaveCloudAnchor = false;
  bool LoggedCloudShadow = false;
  double CloudAnchor[3] = {0, 0, 0};
  double CloudAxisE[3] = {1, 0, 0}, CloudAxisN[3] = {0, 1, 0};

  /* BORROWED, never owned: the vehicle that has the capability owns it, and a body without one leaves
   * the whole avionics layer out of the build rather than out of the frame. */
  OverlayStage *Overlay = nullptr;
  /* Value-initialised at the DECLARATION, not just in the one constructor: RenderFrame reads the
   * weather fields whether or not SetSceneState was ever called, and a second constructor would
   * silently drop that guarantee. The zeros are a DEFINED state — "no weather report", for which the
   * cloud march has its own default deck. */
  State SceneState{};
  bool LoadingScreen = false; float LoadPct = 0.0f; int LoadReady = 0, LoadTotal = 0;   /* boot loading screen */

  double Center[3];   /* terrain field centre — the default orbit camera's fallback target only */
  int MaxLayers = 256;   /* device's real texture-array-layer cap (OnAdapter); handed to Tiles->Configure() */
  bool HaveCamera;                      /* SetCamera used (scripted path) vs the default orbit */
  bool CameraFull;                      /* SetCameraBasis used (full rolled basis) — wins over both */
  double Eye[3], LookTarget[3];
  double Fwd[3], Right[3], Up[3];       /* explicit ECEF camera basis (SetCameraBasis) */
  float FovDeg = 60.0f;                 /* [SET] until a scene declares one; SetFovDeg is the only writer */

  int Width, Height;
  bool DeviceReady, DeviceLost;
  Target Mode;
  bool Blocking;       /* native: RequestAdapter/RequestDevice via Instance::WaitAny, not callbacks */
  const char *Selector;
  unsigned FrameNo;
};

} // namespace outshine::Render
#endif
