#include "Sim.h"

#include "Camera.h"
#include "ClassStructure.h"
#include "ElevationProvider.h"
#include "Ephemeris.h"
#include "Geodesy.h"
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

constexpr int kGroundTries = 200;
constexpr int kAlbedoTileSize = 512;

}  // namespace

Sim::Sim(const Scene &scene, const Assets &assets)
    : Scene_(scene),
      Assets_(assets),
      Wind_(Scene_),
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
}

bool Sim::LoadTables() {
  if (!Mats_.Load(Assets_.GroundMaterials.c_str())) {
    Log::Error("sim", "ground_materials_failed",
               {{"path", Assets_.GroundMaterials}, {"why", Mats_.Error()}});
    return false;
  }
  if (!Veg_.Load(Assets_.Vegetation.c_str(), Mats_)) {
    Log::Error("sim", "vegetation_table_failed",
               {{"path", Assets_.Vegetation}, {"why", Veg_.Error()}});
    return false;
  }
  SunPos(Stance_.Lat, Stance_.Lon, Clk_, &SunEl_, &SunAz_);
  return true;
}

void Sim::StartTelemetry() {
  if (Identity_) Bus_.Register(Identity_);
  Bus_.Register(&Stream_);
}

bool Sim::ResolveGround(double lat, double lon, double *out) const {
  double g = kFBElevationUnresolved;
  for (int t = 0; t < kGroundTries && !ElevationResolved(g); t++) {
    g = fb_stream_ground(lat, lon);
    if (!ElevationResolved(g)) PumpMs(50);
  }
  if (!ElevationResolved(g)) return false;
  *out = g;
  return true;
}

bool Sim::Open() {
  if (Opened_) return false;
  if (!W_.Open(Base_.c_str(), Stance_.Lat, Stance_.Lon, ViewM_, kAlbedoTileSize)) {
    Log::Error("sim", "world_open_failed", {{"base", Base_}});
    return false;
  }
  double ground = 0.0;
  if (!ResolveGround(Stance_.Lat, Stance_.Lon, &ground)) {
    Log::Error("sim", "ground_unresolved",
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

  W_.SetVegetation(&Veg_);
  W_.SetWeather(&Wind_);
  const WindNed w = Wind_.WindNedMs(Stance_.Lat, Stance_.Lon, Stand_.AltAslM());
  Log::Info("sim", "stand", {{"groundM", Stand_.GroundAslM()}, {"eyeM", Stand_.EyeAglM()},
      {"pitchDeg", Stance_.PitchDeg}, {"aslM", Stand_.AltAslM()}, {"liftM", Stand_.LiftM()},
      {"sunElDeg", (double)SunEl_}, {"sunAzDeg", (double)SunAz_},
      {"moonElDeg", (double)State_.Env.MoonElDeg}, {"cloudCover", (double)State_.Env.CloudCover},
      {"windN", w.N}, {"windE", w.E}, {"windD", w.D}});
  Opened_ = true;
  return true;
}

std::optional<World::Forest::Prototype> Sim::GrowTrees() {
  if (Assets_.Species.empty()) return std::nullopt;
  return Forest_.Grow(Assets_.Species.c_str());
}

void Sim::SetSkyOffsetS(double s) {
  const double t = Clk_ + s;
  SunPos(Stance_.Lat, Stance_.Lon, t, &SunEl_, &SunAz_);
  MoonPos(Stance_.Lat, Stance_.Lon, t, &State_.Env.MoonElDeg, &State_.Env.MoonAzDeg,
          &State_.Env.MoonPhase);
  State_.Env.SunElDeg = SunEl_;
  State_.Env.SunAzDeg = SunAz_;
}

void Sim::Look(const Stance &s) {
  Stance_ = s;
  const double g = fb_stream_ground(s.Lat, s.Lon);
  if (ElevationResolved(g)) Stand_.SetGroundAslM(g);
  const double asl = Stand_.AltAslM();
  GeoToEcef(s.Lat, s.Lon, asl, Eye_);
  CameraBasisEcef(s.YawDeg, s.PitchDeg, 0.0, s.Lat, s.Lon, Fwd_, Right_, Up_);
  State_.Platform.AltM = (float)asl;
  State_.Platform.YawDeg = (float)s.YawDeg;
  State_.Platform.PitchDeg = (float)s.PitchDeg;
}

void Sim::Advance() { W_.Update(Stance_.Lat, Stance_.Lon); }

/* THE STANDPOINT IS CHECKED AGAINST WHAT IS DATA AND WHAT IS DRAWN. Terrain and buildings come from
 * DEM and OSM, so the eye is LIFTED above them; a tree is a draw from a landcover density, so a
 * stand whose crown holds the eye is REFUSED (TreeField::Crown). Buildings only exist once the
 * vector tiles have landed, which is why this waits for residency. */
void Sim::Settle() {
  if (!RoofChecked_ && Stand_.LensDeclared()) {
    RoofChecked_ = true;
    const double roof = W_.RoofAslAt(Stance_.Lat, Stance_.Lon);
    const double before = Stand_.AltAslM();
    Stand_.SetRoofAslM(roof);
    if (Stand_.AltAslM() != before) {
      Log::Info("sim", "standpoint_roof", {{"roofAslM", roof},
          {"liftM", Stand_.AltAslM() - before}, {"eyeM", Stand_.EyeAglM()},
          {"totalLiftM", Stand_.LiftM()}});
      Look(Stance_);
    }
  }
  Forest_.Scatter(W_.Classes(), Veg_, Stance_.Lat, Stance_.Lon, Stand_.EyeAglM());
}

Sim::Place Sim::At(double lat, double lon) const {
  Place p;
  const double g = fb_stream_ground(lat, lon);
  p.GroundResolved = ElevationResolved(g);
  if (p.GroundResolved) p.GroundAslM = g;

  const std::shared_ptr<const World::ClassStructure> cls = W_.Classes().Read();
  if (cls) {
    double e = 0.0, n = 0.0;
    W_.Classes().Project(lat, lon, &e, &n);
    int runnerUp = -1;
    p.Class = cls->Evaluate(e, n, &p.ClassEdgeM, &runnerUp);
  }

  const double roof = W_.RoofAslAt(lat, lon);
  if (p.GroundResolved && World::World::SurfaceStands(roof)) p.StructureHeightM = roof - g;
  const double level = W_.WaterAslAt(lat, lon);
  if (p.GroundResolved && World::World::SurfaceStands(level)) p.WaterDepthM = level - g;
  return p;
}

} // namespace outshine::Clients
