/* The WebGPU rendering system, one source and two link targets (emdawnwebgpu / native Dawn).
 * ORCHESTRATOR: it owns device/surface/targets and EVERY Begin/EndRenderPass boundary plus the encode
 * order; drawing lives in DrawStage-derived classes that record into the encoder it already opened.
 * A stage never begins or ends a pass. */
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
#include "Readback.h"
#include "stages/StarsStage.h"
#include "stages/PresentStage.h"
#include "stages/ProgressStage.h"
#include "stages/TransmittanceStage.h"
#include "stages/MultiScatterStage.h"
#include "stages/IrradianceStage.h"
#include "stages/SkyViewStage.h"
#include "stages/SkyStage.h"
#include "stages/SunStage.h"
#include "stages/MoonStage.h"
#include "GeometryStage.h"
#include "stages/BenchGroundStage.h"
#include "stages/ShadowStage.h"
#include "stages/AoStage.h"
#include "stages/ExposureStage.h"
#include "stages/TaaStage.h"
#include "TemporalJitter.h"

namespace outshine::Render {

class Renderer {
public:
  Renderer();

  /* Bring-up into the render target, blocking: there is no event loop to pump callbacks on. */
  void Init(int width, int height);

  [[nodiscard]] bool Ready(void) const { return DeviceReady; }

  /* Acquire the target, run the passes, submit. */
  void RenderFrame(void);

  /* EVERY READER BELOW IS A POLL, and the first call is what issues the transfer: ask each turn,
   * take the answer on the turn it stops being Pending. There is no waiting variant, because the
   * browser's frame thread has no legal way to stand still and a run that stood still there would
   * hold the very event loop its tiles arrive on. */

  /* Tightly packed W*H*4 RGBA8, already sRGB-encoded — ready for a PNG writer. */
  [[nodiscard]] ReadState ReadPixels(std::vector<uint8_t> &rgba);

  /* Ready once everything submitted so far has retired. A per-frame wall clock without it reads the
   * encoder, not the frame. */
  [[nodiscard]] ReadState GpuIdle(void);

  /* Reversed-Z scene depth, W*H floats, row-major. Range along the view ray follows as
   * kNearM / depth / cos(angle off boresight); a critic's "at 1-2 km" is otherwise a guess about a
   * hillside's row. */
  [[nodiscard]] ReadState ReadDepth(std::vector<float> &depth);
  static constexpr float kNearM = 0.05f;   /* MvpCamRel's zn — the numerator of that division */

  /* [sunIrr.rgb, _, skyIrr.rgb, _] in top-of-atmosphere-solar = 1 units: the scale everything lit is
   * multiplied by, measurable instead of asserted. */
  [[nodiscard]] ReadState ReadIrradiance(float out[IrradianceStage::kFloats]);

  /* WHAT THE SCENE MAY DECLARE about its own brightness; Auto with no compensation is the default
   * and needs no declaration. Takes effect on the next frame. */
  void SetExposure(const ExposureParams &p) { Exposure->SetParams(p); }

  /* [expScale, keyLog2, horizE, _]: the scalar the resolve multiplies scene radiance by, the log2 of
   * the radiance it places at middle grey, and the horizontal irradiance it came from. */
  [[nodiscard]] ReadState ReadExposure(float out[ExposureStage::kMeterFloats]);

  /* Enable before Init: the terrain source is per-tile buffers plus a growable CLASS array driven
   * by World. */
  [[nodiscard]] bool DeviceUsable(void) const { return DeviceReady && !DeviceLost; }

  /* A table slot id, or -1 if the device is gone or the class array is full. */
  int  UploadTile(const float *verts, uint32_t nverts, const uint32_t *idx, uint32_t nidx,
                 const DagCluster *clusters, int nclusters,
                 const double origin[3], const double anchor[3]);
  /* THE CLASS STRUCTURE, borrowed for the length of the call (world/ClassField.h). */
  void SetClassFrame(const double east[3], const double north[3], const double camOffset[2]) {
    Geometry->Terrain().SetClassFrame(east, north, camOffset);
  }
  void WriteClassBuffer(const uint32_t *words, size_t bytes) {
    Geometry->Terrain().WriteClassBuffer(words, bytes);
  }
  void ReleaseTile(int slot) { Geometry->Terrain().ReleaseTile(slot); }   /* free the buffer, recycle both layers */
  void SetDrawList(const int *slots, int n) { Geometry->Terrain().SetDrawList(slots, n); }  /* the tiles to draw this frame (World's leaves) */
  long ClassVramBytes(void) const { return Geometry->Terrain().ClassVramBytes(); }   /* the classification input */
  long TileMeshBytes(void) const { return Geometry->Terrain().TileMeshBytes(); }     /* the resident tile geometry */

  /* THE ENVIRONMENT the frame is lit and hazed by — sun, moon, cloud deck, altitude. Nothing to do
   * with a HUD any more: the renderer keeps it because sun/moon/cloud drive its own lighting math. */
  void SetSceneState(const State &s) { SceneState = s; }
  void PinJitter(float x, float y) { Jitter.Pin(x, y); }

  /* A SECOND THING TO DRAW, not a second state to be in. The renderer holds no notion of loading:
   * the caller says which frame it wants this tick, exactly as it says where the camera is. Two
   * passes instead of eight, so the bar keeps full rate while the streams land. §2.2 */
  void RenderProgress(float fraction);

  /* THE VEGETATION TABLE, before Init: 256 resolved rows (world/VegetationTemplates::Row). Without it
   * the terrain keeps its raw albedo — the pre-template picture, on purpose. `bareRockRow` and
   * `slopeBandDeg` come out of the same declaration and travel with it, so the row the ground falls
   * back to on a wall can never name a table that is not loaded. */
  void SetVegetationTable(const void *rows, size_t rowBytes, int bareRockRow, float slopeBandDeg);

  /* WHERE THE GROUND COVER IS READ. The ground fragment IS the cover (render/Sward.h), so it needs
   * the place and the local basis and nothing else — the geodetic position picks the graticule cell
   * the field is hashed on. */
  void SetSwardBasis(double lat, double lon, const double east[3], const double north[3],
                     const double up[3]) {
    Geometry->Terrain().SetSward(lat, lon, east, north, up);
  }
  /* THE SUBJECT BENCH'S FLOOR AND ITS NEUTRAL CARD — studio furniture, and off unless a bench
   * declares it. `radiusM` <= 0 retires the floor and a card
   * of zero width retires the card, which is the state every other client is in for the whole of its
   * life. */
  void SetBenchGround(double eyeAglM, double radiusM, double gridM) {
    BenchGround->SetPlane(eyeAglM, radiusM, gridM);
  }
  void SetBenchSubstrate(const float linearRgb[3]) { BenchGround->SetSubstrate(linearRgb); }
  void SetBenchCard(const BenchCard &card) { BenchGround->SetCard(card); }

  /* AN INSTANCED PROTOTYPE, in the same camera-relative ground frame the bench's floor and card use.
   * The mesh arrives as raw arrays and the material as a ROW OF NUMBERS whose meanings are pinned in
   * the shader that reads it: render/ draws what it is handed and knows nothing about what the model
   * is of. `SetPrototypeHeightM` <= 0 with no instance field is the state every client stays in
   * until one is uploaded. */
  void SetPrototypeLevel(int level, const LevelMesh &mesh) {
    Geometry->Models().SetLevel(level, mesh);
  }
  void SetPrototypeDetail(const DetailMesh &detail) { Geometry->Models().SetDetail(detail); }
  void SetPrototypeSheets(int level, const SheetSet &sheets) {
    Geometry->Models().SetSheets(level, sheets);
  }
  void SetPrototypeMaterial(const float row[kMaterialRowFloats]) {
    Geometry->Models().SetMaterial(row);
  }
  void SetPrototypeHeightM(double heightM) { Geometry->Models().SetHeightM(heightM); }
  void SetPrototypeBounds(const ModelBounds &bounds) { Geometry->Models().SetBounds(bounds); }
  void SetPrototypeSubject(double eastM, double northM, double eyeAglM) {
    Geometry->Models().SetSubject(eastM, northM, eyeAglM);
  }
  void SetPrototypeInstances(const float *instances, uint32_t n, const TangentFrame &frame) {
    Geometry->Models().SetInstances(instances, n, frame);
  }
  void SetSheetsVisible(bool on) { Geometry->Models().SetSheetsVisible(on); }
  /* THE ONE PASS THAT IS NOT PER FRAME. Renderer opens every render pass, and the impostor atlas is
   * baked ONCE at load into its own command buffer — the per-frame pass count is untouched. */
  void BakePrototypeImpostor(void);
  long ModelMeshInstances() const { return Geometry->Models().MeshInstances(); }
  double ModelMeshRadiusM() const { return Geometry->Models().MeshRadiusM(); }
  long ModelImpostorInstances(void) const { return Geometry->Models().ImpostorInstances(); }
  long ModelLevelInstances(int level) const { return Geometry->Models().LevelInstances(level); }
  long ModelTriangleCount(void) const { return Geometry->Models().TriangleCount(); }
  size_t ModelPrototypeBytes() const { return Geometry->Models().PrototypeBytes(); }
  size_t ModelInstanceBytes() const { return Geometry->Models().InstanceBytes(); }
  size_t ModelImpostorBytes() const { return Geometry->Models().ImpostorBytes(); }

  /* THE DECLARED SUBJECT OF A STUDIO (stages/SubjectDraw.h): one indexed position-only mesh, in
   * ECEF offsets from `anchor`. A client that never declares one draws nothing extra. */
  void SetSubjectMesh(const float *verts, uint32_t nverts, const uint32_t *idx, uint32_t nidx,
                      const double anchor[3]) {
    Geometry->Subjects().SetMesh(verts, nverts, idx, nidx, anchor);
  }
  long SubjectTriangleCount() const { return Geometry->Subjects().TriangleCount(); }

  /* THE SCENE'S DECLARED WIND, met convention (the bearing it comes from, m/s at 10 m). It is held
   * for the consumers that owe a published anchor and read by no stage today. */
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

  /* THE PER-PASS CLOCK, for whoever aggregates it. False means the device refused the feature, and
   * that is a different statement from eight zeroes. */
  [[nodiscard]] bool GpuTimingAvailable(void) const { return GpuTimeGranted; }
  [[nodiscard]] bool TakeGpuTimes(GpuTimer::Sample &out) { return GpuTime.Take(out); }

  /* The extruded OSM footprints World decoded; positions are ECEF offsets from `anchor`. */
  void SetBuildingMesh(const float *verts, uint32_t nverts, const uint32_t *idx, uint32_t nidx,
                       const DagCluster *clusters, int nclusters,
                       const double anchor[3]) {
    Geometry->Buildings().SetMesh(verts, nverts, idx, nidx, clusters, nclusters, anchor);
  }
  uint32_t BuildingVertexCount(void) const { return Geometry->Buildings().VertexCount(); }
  void SetWaterMesh(const float *v, uint32_t n, const double anchor[3]) {
    Geometry->Water().SetMesh(v, n, anchor);
  }
  uint32_t WaterVertexCount(void) const { return Geometry->Water().VertexCount(); }

  int  DrawCount(void) const { return Geometry->Terrain().DrawCount(); }   /* tile draws/frame (TileBuf = n*32 B) */
  int  TerrainDrawCalls(void) const { return Geometry->Terrain().DrawCallCount(); }
  const long *TerrainTrianglesByLevel(void) const { return Geometry->Terrain().TrianglesByLevel(); }
  long ShadowTriangleCount(void) const { return Shadow->TriangleCount(); }
  int  ShadowDrawCalls(void) const { return Shadow->DrawCallCount(); }
  int  TerrainVisibleTiles(void) const { return Geometry->Terrain().VisibleTileCount(); }
  long TerrainTriangleCount(void) const { return Geometry->Terrain().TriangleCount(); }
  /* The two accumulation buffers plus the motion attachment — the whole price of TAA in memory. */
  long TemporalVramBytes(void) const {
    return Taa->HistoryBytes() + (long)Width * (long)Height * 4L;
  }

  /* THE BUDGET INSTRUMENT: triangles the last frame submitted, over every geometry unit there is. */
  long TriangleCount(void) const { return Geometry->TriangleCount(); }

  /* Call before Init; without it the moon disc falls back to flat grey. */
  void SetMoonTexture(const uint8_t *rgba, int w, int h);

  /* Multiplier over the true angular radius (1 = realistic ~0.5 deg, ~5 px at 720p). */
  void SetMoonScale(double s) { MoonScale = s > 0.0 ? s : 1.0; }

  /* Scales the march step counts; 0 disables the cloud pass entirely. */


  /* 6 B/star, mag-sorted. Call before Init; placed at true alt/az for the given origin. */
  void SetStars(const uint8_t *hyg, int nbytes, double originLat, double originLon);

  /* Drives sidereal star placement. */
  void SetSkyClock(double unixSec) { SkyClock = unixSec; }

  /* Up is derived radial: no roll. */
  void SetCamera(const double eye[3], const double target[3]);

  /* Carries ROLL, so the horizon tilts at bank. Takes precedence over SetCamera/orbit. */
  void SetCameraBasis(const double eye[3], const double fwd[3], const double right[3],
                      const double up[3]);

  /* THE SCENE'S vertical field of view over the full frame height, as the scene file declares it.
   * Call before the first frame; it enters the projection and the atmosphere uniform together, so
   * there is never a second copy to drift from. */
  void SetFovDeg(double deg) { FovDeg = deg > 0.0 ? (float)deg : FovDeg; }
  void SetOrthoM(double m) { OrthoM = (float)m; }
  float GetFovDeg(void) const { return FovDeg; }

  int SceneW(void) const { return Width; }
  int SceneH(void) const { return Height; }

  const wgpu::Device &GetDevice(void) const { return Device; }
  wgpu::TextureFormat GetSurfaceFormat(void) const { return SurfaceFormat; }

private:
  void CreateTerrainPipeline(void);   /* DepthTex/HdrTex (shared scene targets) + the terrain unit */
  void CreateResolvePipeline(void);   /* AO, the meter, and the resolve that carries the display curve */
  void CreatePresent(void);           /* declared-size frame target + the present pass over it */
  [[nodiscard]] bool AcquireTarget(wgpu::TextureView &finalView);   /* the one place a presentable target comes from */
  void EncodePresent(wgpu::CommandEncoder &enc, const wgpu::TextureView &finalView,
                     const FrameContext &ctx, wgpu::PassTimestampWrites *timestamps);
  wgpu::Instance MakeInstance(void) const;   /* the descriptor, and the native path asks for one more */
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
  void CreateOffscreenTarget(void);

  wgpu::Instance Instance;
  wgpu::Adapter Adapter;
  wgpu::Device Device;
  wgpu::Queue Queue;   /* cached once — per-frame GetQueue() churns wrapper refs (measured: device died one ref per frame) */
  wgpu::Texture OffscreenTex;   /* the final colour target: RGBA8UnormSrgb, RenderAttachment|CopySrc */
  wgpu::TextureFormat SurfaceFormat;   /* what pipelines and views use: always sRGB-encoding */
  wgpu::TextureFormat HdrFormat;   /* offscreen scene target: rg11b10ufloat where renderable, else rgba16float */
  /* Sun shadows and contact occlusion. Both own a target and neither owns a pass: the shadow atlas is
   * filled in a pass Renderer opens before the scene, the AO buffer in one it opens after. */
  std::unique_ptr<AoStage> Ao = std::make_unique<AoStage>();
  std::unique_ptr<ExposureStage> Exposure = std::make_unique<ExposureStage>();
  wgpu::Sampler Samp;              /* shared linear sampler: unit textures, TAA history, tonemap's HdrTex read */
  wgpu::Texture DepthTex, HdrTex;  /* shared scene targets: the geometry stage and SkyStage draw into them */
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
  /* ONE GEOMETRY STAGE. Terrain, prisms, water and instanced models, over one cluster cut. */
  std::unique_ptr<GeometryStage> Geometry = std::make_unique<GeometryStage>();
  /* The 256 vegetation-template rows: the terrain shades its ground cover out of them. */
  wgpu::Buffer VegBuf;
  std::vector<uint8_t> VegRows;
  std::unique_ptr<BenchGroundStage> BenchGround = std::make_unique<BenchGroundStage>();
  std::unique_ptr<ShadowStage> Shadow = std::make_unique<ShadowStage>();
  wgpu::Buffer CsmBuf;

  /* The whole frame lands in a FrameTex of the DECLARED size, and the present pass copies it on. */
  wgpu::Texture FrameTex;
  std::unique_ptr<PresentStage> Present = std::make_unique<PresentStage>();
  std::unique_ptr<ProgressStage> Progress = std::make_unique<ProgressStage>();

  /* Hillaire 2020. These resources stay Renderer-owned because 3+ consumers read them; the stages
   * hold only the pipeline/bind group built from views injected at Configure(). §4 */
  wgpu::Texture TransLUT, MsLUT, SkyLUT;          /* 256x64, 32x32, 192x108 rgba16float (storage + sampled) */
  wgpu::Sampler LutSamp;                          /* linear, U-repeat (azimuth wraps), V-clamp */
  std::vector<TerrainDraw::Caster> TerrainCasters;
  std::vector<ShadowStage::TerrainCaster> ShadowTerrain;        /* reused, so a steady scene allocates nothing */
  GpuTimer GpuTime;                               /* per-pass GPU time, telemetry */
  bool GpuTimeGranted = false;
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

  /* THE cloud chain: one stage, one pass, straight into HdrTex. */

  /* THE ONE cloud field, as a buffer: the march reads it and so does every lit surface, because a
   * shadow computed from a second field would not lie under its cloud. Renderer-owned for the same
   * reason AtmoBuf is — four consumers, one writer. */
  double CloudQuality = 0.0;   /* 0 = the sheet; > 0 drives the volumetric march (stages/clouds.md) */
  /* The field's horizontal origin is a PLACE (SkyParams.Anchor*), never the camera: pinning it at the
   * first eye made the cloud pattern a function of where the session started. */
  bool HaveCloudAnchor = false;
  bool LoggedCloudShadow = false;
  double CloudAnchor[3] = {0, 0, 0};
  double CloudAxisE[3] = {1, 0, 0}, CloudAxisN[3] = {0, 1, 0};

  /* Value-initialised at the DECLARATION, not just in the one constructor: RenderFrame reads the
   * weather fields whether or not SetSceneState was ever called, and a second constructor would
   * silently drop that guarantee. The zeros are a DEFINED state — "no weather report", for which the
   * cloud march has its own default deck. */
  State SceneState{};

  double Center[3];   /* terrain field centre — the default orbit camera's fallback target only */
  int MaxLayers = 256;   /* device's real texture-array-layer cap (OnAdapter); handed to the terrain unit */
  bool HaveCamera;                      /* SetCamera used (scripted path) vs the default orbit */
  bool CameraFull;                      /* SetCameraBasis used (full rolled basis) — wins over both */
  double Eye[3], LookTarget[3];
  double Fwd[3], Right[3], Up[3];       /* explicit ECEF camera basis (SetCameraBasis) */
  float FovDeg = 60.0f;                 /* [SET] until a scene declares one; SetFovDeg is the only writer */
  float OrthoM = 0.0f;                  /* > 0: parallel projection covering this many metres */

  Readback PixelRead, DepthRead, IrrRead, MeterRead;
  bool WorkWatched = false, WorkRetired = false;

  int Width, Height;
  bool DeviceReady, DeviceLost;
  unsigned FrameNo;
};

} // namespace outshine::Render
#endif
