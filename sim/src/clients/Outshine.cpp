#include "Outshine.h"

#include <chrono>
#include <cstdlib>

#include "Camera.h"
#include "ElevationProvider.h"
#include "Ephemeris.h"
#include "Geodesy.h"
#include "HeapProbe.h"
#include "Log.h"
#include "PixelFocalLength.h"
#include "StackProbe.h"
#include "TerrainLoader.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace outshine::Clients {
namespace {

/* A DEM answer arrives synchronously off a native fetch and asynchronously in the browser, and the
 * boot needs one before it can place the eye. Waiting it out is the only honest option: an invented
 * plateau would move the whole picture, and this is the only picture there is. */
void PumpMs(int ms) {
#ifdef __EMSCRIPTEN__
  emscripten_sleep((unsigned)ms);
#else
  (void)ms;
#endif
}

/* [SET] 10 s without one more ready tile. Long enough that the slowest single tile measured on this
 * host (a cold z14 vector bake, ~3 s) never trips it, short enough to name a hung server. */
constexpr double kStallSayMs = 10000.0;

constexpr int kGroundTries = 200;
constexpr int kDeviceTries = 2000;
constexpr int kAlbedoTileSize = 512;
constexpr int kStarBytes = 262144;

double NowMs() {
  return std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

}  // namespace

Outshine::Outshine(const Scene &scene, const Assets &assets)
    : Scene_(scene),
      Assets_(assets),
      Wind_(Scene_),
      Exposure_(Scene_.Exposure()),
      Stance_{Scene_.Lat(), Scene_.Lon(), Scene_.YawDeg(), Scene_.PitchDeg()},
      WindDeg_(Scene_.WindDeg()),
      WindMs_(Scene_.WindMs()),
      Clk_((double)Scene_.UtcS()),
      W_(PixelFocalLength(Scene_.RenderResolution().Height, Scene_.FovDeg())) {
  /* The thread that builds this object is the one that will draw on it, and this is the earliest
   * moment at which the engine can say so. */
  StackProbe::Enter(StackProbe::Purpose::Frame);
  ViewM_ = Scene_.ViewM();
  OrthoM_ = Scene_.OrthoM();
  Stand_.SetEyeAglM(Scene_.EyeM());
  if (Scene_.HasLensAslM()) Stand_.SetLensAslM(Scene_.LensAslM());
  if (Scene_.HasJitterPin()) R_.PinJitter((float)Scene_.JitterPinX(), (float)Scene_.JitterPinY());
}

void Outshine::Pump() { PumpMs(0); }

void Outshine::SetFovDeg(double deg) {
  R_.SetFovDeg(deg);
  W_.SetPixelFocalLength(PixelFocalLength(Scene_.RenderResolution().Height, deg));
}

void Outshine::SetExposureCompEv(double ev) {
  Exposure_.CompEv = (float)ev;
  R_.SetExposure(Exposure_);
}

/* THE SKY CLOCK MOVES, the wind clock is a different one (SetWindClock). Everything the sun and the
 * moon reach from here is a setter; nothing below re-bakes a tile, which is why this is the only
 * clock a run may drive today. */
void Outshine::SetSkyOffsetS(double s) {
  const double t = Clk_ + s;
  SunPos(Stance_.Lat, Stance_.Lon, t, &SunEl_, &SunAz_);
  MoonPos(Stance_.Lat, Stance_.Lon, t, &State_.Env.MoonElDeg, &State_.Env.MoonAzDeg,
          &State_.Env.MoonPhase);
  State_.Env.SunElDeg = SunEl_;
  State_.Env.SunAzDeg = SunAz_;
  R_.SetSkyClock(t);
  R_.SetSceneState(State_);
}

bool Outshine::ResolveGround(double lat, double lon, double *out) const {
  double g = kFBElevationUnresolved;
  for (int t = 0; t < kGroundTries && !ElevationResolved(g); t++) {
    g = fb_stream_ground(lat, lon);
    if (!ElevationResolved(g)) PumpMs(50);
  }
  if (!ElevationResolved(g)) return false;
  *out = g;
  return true;
}

bool Outshine::Prepare(const Gpu &gpu) {
  if (Phase_ != Phase::Declared) return false;
  SunPos(Stance_.Lat, Stance_.Lon, Clk_, &SunEl_, &SunAz_);
  Log::Info("outshine", "scene", {{"scene", Scene_.Id()}, {"lat", Stance_.Lat},
      {"lon", Stance_.Lon}, {"eyeM", Stand_.EyeAglM()}, {"yawDeg", Stance_.YawDeg},
      {"pitchDeg", Stance_.PitchDeg}, {"fovDeg", Scene_.FovDeg()}, {"utc", Scene_.Utc()},
      {"utcS", Clk_}, {"windDeg", WindDeg_}, {"windMs", WindMs_},
      {"cloudCover", Scene_.CloudCover()}, {"sunElDeg", (double)SunEl_},
      {"sunAzDeg", (double)SunAz_}});
  /* Only a DEVIATION is an event: at the budget size there is nothing to justify. */
  const Scene::Resolution &res = Scene_.RenderResolution();
  if (!res.Why.empty())
    Log::Info("outshine", "render_size", {{"width", res.Width}, {"height", res.Height},
        {"why", res.Why}});

  if (!Mats_.Load(Assets_.GroundMaterials.c_str())) {
    Log::Error("outshine", "ground_materials_failed",
               {{"path", Assets_.GroundMaterials}, {"why", Mats_.Error()}});
    return false;
  }
  if (!Veg_.Load(Assets_.Vegetation.c_str(), Mats_)) {
    Log::Error("outshine", "vegetation_table_failed",
               {{"path", Assets_.Vegetation}, {"why", Veg_.Error()}});
    return false;
  }

  R_.SetVegetationTable(Veg_.Rows(), Veg_.RowBytes(), Veg_.RockTemplate(),
                        Veg_.Limit().SlopeBandDeg());
  R_.SetSkyClock(Clk_);
  SetFovDeg(Scene_.FovDeg());
  R_.SetOrthoM(OrthoM_);
  R_.SetWind(WindDeg_, WindMs_);
  R_.SetExposure(Exposure_);

  if (gpu.Canvas) R_.Init(gpu.Canvas, res.Width, res.Height);
  else R_.InitOffscreen(res.Width, res.Height);
  /* THE DEVICE HAS TO BE THERE BEFORE ANYTHING UPLOADS: a stage's Upload returns nothing without one
   * and drops its geometry in silence. Native Dawn is already up; the browser's request is a
   * promise. */
  for (int t = 0; t < kDeviceTries && !R_.Ready(); t++) PumpMs(10);
  if (!R_.Ready()) {
    Log::Error("outshine", "device_init_failed");
    return false;
  }
  Frames_.SetGpuAvailable(R_.GpuTimingAvailable());
  if (Identity_) Bus_.Register(Identity_);
  Bus_.Register(&Frames_);
  Bus_.Register(&Stream_);
  Bus_.Register(&Memory_);
  Bus_.Start();
  ClockOriginMs_ = NowMs();
  Frames_.Open(ClockOriginMs_);
  Stream_.Open(ClockOriginMs_);
  Phase_ = Phase::Prepared;
  return true;
}

bool Outshine::Open() {
  if (Phase_ != Phase::Prepared) return false;
  if (!Assets_.Moon.empty()) {
    uint8_t *rgba = nullptr;
    int w = 0, h = 0;
    if (fb_load_image_file(Assets_.Moon.c_str(), &rgba, &w, &h)) {
      R_.SetMoonTexture(rgba, w, h);
      free(rgba);
    } else {
      Log::Warn("outshine", "moon_texture_missing", {{"path", Assets_.Moon}});
    }
  }
  {
    static uint8_t stars[kStarBytes];
    const int n = fb_fetch_stars(Base_.c_str(), stars, kStarBytes);
    if (n > 0) R_.SetStars(stars, n, Stance_.Lat, Stance_.Lon);
    else Log::Warn("outshine", "star_catalogue_unreachable", {{"base", Base_}});
  }

  if (!W_.Open(&R_, Base_.c_str(), Stance_.Lat, Stance_.Lon, ViewM_, kAlbedoTileSize)) {
    Log::Error("outshine", "world_open_failed", {{"base", Base_}});
    return false;
  }

  double ground = 0.0;
  if (!ResolveGround(Stance_.Lat, Stance_.Lon, &ground)) {
    Log::Error("outshine", "ground_unresolved",
               {{"lat", Stance_.Lat}, {"lon", Stance_.Lon}, {"base", Base_}});
    return false;
  }
  Stand_.SetGroundAslM(ground);

  State_.Env.SunElDeg = SunEl_;
  State_.Env.SunAzDeg = SunAz_;
  MoonPos(Stance_.Lat, Stance_.Lon, Clk_, &State_.Env.MoonElDeg, &State_.Env.MoonAzDeg,
          &State_.Env.MoonPhase);
  State_.Env.CloudCover = (float)Scene_.CloudCover();
  Look(Stance_);

  if (!Assets_.Species.empty() && !Forest_.Grow(R_, Assets_.Species.c_str())) return false;

  W_.SetVegetation(&Veg_);
  W_.SetWeather(&Wind_);
  const WindNed w = Wind_.WindNedMs(Stance_.Lat, Stance_.Lon, Stand_.AltAslM());
  Log::Info("outshine", "stand", {{"groundM", Stand_.GroundAslM()}, {"eyeM", Stand_.EyeAglM()},
      {"pitchDeg", Stance_.PitchDeg}, {"aslM", Stand_.AltAslM()}, {"liftM", Stand_.LiftM()},
      {"sunElDeg", (double)SunEl_}, {"sunAzDeg", (double)SunAz_},
      {"moonElDeg", (double)State_.Env.MoonElDeg}, {"cloudCover", (double)State_.Env.CloudCover},
      {"windN", w.N}, {"windE", w.E}, {"windD", w.D}});
  Phase_ = Phase::Loading;
  return true;
}

void Outshine::SetWindClock(double s) { R_.SetWindClock(s); }

/* The interval between two frames, not the encode: what a viewer feels is the period, and everything
 * the client did in between — streaming, a readback, a PNG — is part of it. A progress frame is
 * metered by the same pair, because there is no measurement mode: whether a second was spent loading
 * is a FIELD of its row (StreamTelemetry.h), never a reason to leave it out. */
double Outshine::OpenFrame() {
  const double now = NowMs();
  if (LastFrameMs_ > 0.0) Frames_.AddFrame(now - LastFrameMs_);
  LastFrameMs_ = now;
  return now;
}

void Outshine::CloseFrame(double startedMs) {
  HeapProbe::Sample();
  double stage[Render::GpuTimer::kPassCount];
  if (R_.TakeGpuTimes(stage)) Frames_.AddStages(stage);
  if (!Frames_.Due(startedMs)) return;
  Bus_.Tick((startedMs - ClockOriginMs_) * 0.001);
  Frames_.Reset(startedMs);
  Stream_.Reset();
}

void Outshine::Frame() {
  const double now = OpenFrame();
  R_.RenderFrame();
  CloseFrame(now);
}

Outshine::Counters Outshine::Measured() const {
  Counters c;
  c.Draws = R_.DrawCount();
  c.Triangles = (long)R_.TriangleCount();
  c.TreeTriangles = R_.TreeTriangleCount();
  c.TreeStands = Forest_.StandCount();
  c.BuildingVerts = R_.BuildingVertexCount();
  c.Built = W_.BuiltCount();
  c.WorldMs = W_.UpdateMs();
  c.MeshMs = W_.MeshMs();
  c.AlbedoMs = W_.AlbedoMs();
  c.UploadMs = W_.UploadMs();
  c.BuildingMs = W_.BuildingMs();
  c.BuildingDecodeMs = W_.BuildingDecodeMs();
  c.GroundAslM = Stand_.GroundAslM();
  c.AltAslM = Stand_.AltAslM();
  c.Fraction = W_.LoadProgress();
  c.Resident = W_.Resident();
  return c;
}

void Outshine::Look(const Stance &s) {
  Stance_ = s;
  const double g = fb_stream_ground(s.Lat, s.Lon);
  if (ElevationResolved(g)) Stand_.SetGroundAslM(g);
  const double asl = Stand_.AltAslM();
  GeoToEcef(s.Lat, s.Lon, asl, Eye_);
  CameraBasisEcef(s.YawDeg, s.PitchDeg, 0.0, s.Lat, s.Lon, Fwd_, Right_, Up_);
  R_.SetCameraBasis(Eye_, Fwd_, Right_, Up_);
  State_.Platform.AltM = (float)asl;
  State_.Platform.YawDeg = (float)s.YawDeg;
  State_.Platform.PitchDeg = (float)s.PitchDeg;
  R_.SetSceneState(State_);
}

/* THE STANDPOINT IS CHECKED AGAINST WHAT IS DATA AND WHAT IS DRAWN. Terrain and buildings come from
 * DEM and OSM, so the eye is LIFTED above them; a tree is a draw from a landcover density, so a
 * stand whose crown holds the eye is REFUSED (TreeField::Crown). Buildings only exist once the
 * vector tiles have landed, which is why this waits for residency. */
void Outshine::CheckRoof() {
  RoofChecked_ = true;
  const double roof = W_.RoofAslAt(Stance_.Lat, Stance_.Lon);
  const double before = Stand_.AltAslM();
  Stand_.SetRoofAslM(roof);
  if (Stand_.AltAslM() == before) return;
  Log::Info("outshine", "standpoint_roof", {{"roofAslM", roof},
      {"liftM", Stand_.AltAslM() - before}, {"eyeM", Stand_.EyeAglM()},
      {"totalLiftM", Stand_.LiftM()}});
  Look(Stance_);
}

Outshine::Progress Outshine::Stream(double nowMs) {
  if (Phase_ < Phase::Loading) return {};
  W_.Update(Stance_.Lat, Stance_.Lon, Eye_, Fwd_, nowMs);
  if (W_.Resident()) {
    if (!RoofChecked_ && Stand_.LensDeclared()) CheckRoof();
    Forest_.Scatter(R_, W_.Classes(), Veg_, Stance_.Lat, Stance_.Lon, Stand_.EyeAglM());
  }
  StreamTelemetry::Pass p;
  p.WorldMs = W_.UpdateMs();
  p.MeshMs = W_.MeshMs();
  p.AlbedoMs = W_.AlbedoMs();
  p.UploadMs = W_.UploadMs();
  p.BuildingMs = W_.BuildingMs();
  p.BuildingDecodeMs = W_.BuildingDecodeMs();
  p.ClassMs = W_.ClassMs();
  p.TilesTotal = W_.TargetTotal();
  p.TilesReady = W_.TargetReadyN();
  p.TilesInView = W_.TargetInViewN();
  p.VectorTilesPending = W_.BuildingPendingTiles();
  p.Built = W_.BuiltCount();
  p.Evicted = W_.EvictedCount();
  p.Resident = W_.Resident();
  Stream_.AddPass(p);
  return {W_.LoadProgress(), W_.Resident()};
}

/* THE PROGRESS FRAME IS A FRAME LIKE ANY OTHER, and that is the whole point: the renderer never
 * stops, so the bar moves at the display's rate while the fetches and the decodes run beside it.
 * What the loop does NOT do is draw the world — a half-arrived scene is not a picture of anything.
 * The virtual clock is the pass index at 60 Hz, so the world's own 1 Hz counters keep the same
 * meaning they have inside a run. */
bool Outshine::Load() {
  if (Phase_ != Phase::Loading) return false;
  Progress p;
  long passes = 0;
  const double t0 = NowMs();
  double movedMs = t0, saidMs = t0;
  int wasReady = -1;
  while (!p.Resident) {
    p = Stream((double)passes * 1000.0 / 60.0);
    const double frameMs = OpenFrame();
    R_.RenderProgress(p.Fraction);
    CloseFrame(frameMs);
    Pump();
    passes++;
    /* A STALL IS SAID, NOT CAPPED. There is no ceiling to hit — a server that stops answering is a
     * fact about the server — but a load that spins in silence is a fact nobody can read. */
    const double now = NowMs();
    if (W_.TargetReadyN() != wasReady) { wasReady = W_.TargetReadyN(); movedMs = now; }
    if (now - movedMs > kStallSayMs && now - saidMs > kStallSayMs) {
      saidMs = now;
      Log::Warn("outshine", "load_stalled", {{"stalledS", (now - movedMs) * 0.001},
          {"passes", (double)passes}, {"targetReady", wasReady},
          {"targetTotal", W_.TargetTotal()}, {"vectorPending", W_.BuildingPendingTiles()},
          {"progress", (double)p.Fraction}});
    }
  }
  Stream_.MarkResident(NowMs());
  Phase_ = Phase::Playing;
  Log::Info("outshine", "loaded", {{"passes", (double)passes}, {"loadMs", NowMs() - t0},
      {"targetTotal", W_.TargetTotal()}, {"targetInView", W_.TargetInViewN()},
      {"progress", (double)p.Fraction}});
  return true;
}

} // namespace outshine::Clients
