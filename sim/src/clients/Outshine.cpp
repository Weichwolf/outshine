#include "Outshine.h"

#include <chrono>
#include <cstdlib>

#include "Camera.h"
#include "ElevationProvider.h"
#include "Ephemeris.h"
#include "Geodesy.h"
#include "Log.h"
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
      Clk_((double)Scene_.UtcS()) {
  ViewM_ = Scene_.ViewM();
  OrthoM_ = Scene_.OrthoM();
  Stand_.SetEyeAglM(Scene_.EyeM());
  if (Scene_.HasLensAslM()) Stand_.SetLensAslM(Scene_.LensAslM());
}

void Outshine::SetFovDeg(double deg) { R_.SetFovDeg(deg); }

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
  W_.SetSunElevationDeg(SunEl_);
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

  R_.SetVegetationTable(Veg_.Rows(), Veg_.RowBytes(), Veg_.BareRockTemplate(),
                        Veg_.Limit().SlopeBandDeg());
  R_.SetSkyClock(Clk_);
  R_.SetFovDeg(Scene_.FovDeg());
  R_.SetOrthoM(OrthoM_);
  R_.SetWind(WindDeg_, WindMs_);
  R_.SetExposure(Exposure_);

  if (gpu.Canvas) R_.Init(gpu.Canvas, gpu.Width, gpu.Height);
  else R_.InitOffscreen(gpu.Width, gpu.Height);
  /* THE DEVICE HAS TO BE THERE BEFORE ANYTHING UPLOADS: a stage's Upload returns nothing without one
   * and drops its geometry in silence. Native Dawn is already up; the browser's request is a
   * promise. */
  for (int t = 0; t < kDeviceTries && !R_.Ready(); t++) PumpMs(10);
  if (!R_.Ready()) {
    Log::Error("outshine", "device_init_failed");
    return false;
  }
  Frames_.SetGpuAvailable(R_.GpuTimingAvailable());
  Bus_.Register(&Frames_);
  Bus_.Start();
  ClockOriginMs_ = NowMs();
  Frames_.Open(ClockOriginMs_);
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

  State_.Platform.Mode = Mode::Manual;
  State_.Env.SunElDeg = SunEl_;
  State_.Env.SunAzDeg = SunAz_;
  MoonPos(Stance_.Lat, Stance_.Lon, Clk_, &State_.Env.MoonElDeg, &State_.Env.MoonAzDeg,
          &State_.Env.MoonPhase);
  State_.Env.CloudCover = (float)Scene_.CloudCover();
  Look(Stance_);

  if (!Assets_.Species.empty() && !Forest_.Grow(R_, Assets_.Species.c_str())) return false;

  W_.SetVegetation(&Veg_);
  W_.SetSunElevationDeg(SunEl_);
  W_.SetWeather(&Wind_);
  const WindNed w = Wind_.WindNedMs(Stance_.Lat, Stance_.Lon, Stand_.AltAslM());
  Log::Info("outshine", "stand", {{"groundM", Stand_.GroundAslM()}, {"eyeM", Stand_.EyeAglM()},
      {"pitchDeg", Stance_.PitchDeg}, {"aslM", Stand_.AltAslM()}, {"liftM", Stand_.LiftM()},
      {"sunElDeg", (double)SunEl_}, {"sunAzDeg", (double)SunAz_},
      {"moonElDeg", (double)State_.Env.MoonElDeg}, {"cloudCover", (double)State_.Env.CloudCover},
      {"windN", w.N}, {"windE", w.E}, {"windD", w.D}});
  Phase_ = Phase::Streaming;
  return true;
}

void Outshine::SetWindClock(double s) { R_.SetWindClock(s); }

/* The interval between two Frame() calls, not the encode: what a viewer feels is the period, and
 * everything the client did in between — streaming, a readback, a PNG — is part of it. */
void Outshine::Frame() {
  const double now = NowMs();
  if (LastFrameMs_ > 0.0) Frames_.AddFrame(now - LastFrameMs_);
  LastFrameMs_ = now;
  R_.RenderFrame();
  double stage[Render::GpuTimer::kPassCount];
  if (R_.TakeGpuTimes(stage)) Frames_.AddStages(stage);
  if (Frames_.Due(now)) {
    Bus_.Tick((now - ClockOriginMs_) * 0.001);
    Frames_.Reset(now);
  }
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
  if (Phase_ != Phase::Streaming) return {};
  W_.Update(Stance_.Lat, Stance_.Lon, Eye_, Fwd_, nowMs);
  if (W_.Resident()) {
    if (!RoofChecked_ && Stand_.LensDeclared()) CheckRoof();
    Forest_.Scatter(R_, W_.Classes(), Veg_, Stance_.Lat, Stance_.Lon, Stand_.EyeAglM());
  }
  return {W_.LoadProgress(), W_.Resident()};
}

} // namespace outshine::Clients
