/* THE SYSTEM. One declared scene in, one image out — and it is the ONLY thing that builds a world:
 * every client is an entry point plus an output medium over this object, so what one client shows
 * the other cannot fail to show. */
#ifndef OUTSHINE_H
#define OUTSHINE_H

#include <string>

#include "ExposureParams.h"
#include "FrameTelemetry.h"
#include "Forest.h"
#include "GroundMaterials.h"
#include "Renderer.h"
#include "Scene.h"
#include "SceneWeather.h"
#include "Standpoint.h"
#include "State.h"
#include "Telemetry.h"
#include "VegetationTemplates.h"
#include "World.h"

namespace outshine::Clients {

class Outshine {
public:
  /* The engine's own declarations, by path, because the two toolchains mount them differently — a
   * preloaded virtual FS in the browser, the working directory natively. Nothing here is content. */
  struct Assets {
    std::string Vegetation, GroundMaterials, Species, Moon;
  };
  struct Gpu {
    const char *Canvas = nullptr;   /* null = offscreen */
    int Width = 1280, Height = 720;
  };
  struct Stance {
    double Lat = 0.0, Lon = 0.0, YawDeg = 0.0, PitchDeg = 0.0;
  };
  struct Progress {
    float Fraction = 0.0f;
    bool Resident = false;
  };
  /* WHAT A CLIENT MAY REPORT. An entry point states what it saw without reaching into the peers to
   * find out — a hand into Renderer is a hand that can also build (make verify-clients). */
  struct Counters {
    int Draws = 0;
    long Triangles = 0, TreeTriangles = 0, TreeStands = 0;
    unsigned BuildingVerts = 0;
    long Built = 0;
    double WorldMs = 0.0, MeshMs = 0.0, AlbedoMs = 0.0, UploadMs = 0.0;
    double BuildingMs = 0.0, BuildingDecodeMs = 0.0;
    double GroundAslM = 0.0, AltAslM = 0.0;
    float Fraction = 0.0f;
    bool Resident = false;
  };
  /* A DEVICE IS A PROMISE AND A TILE SERVER IS A NETWORK, so bring-up cannot finish in a
   * constructor. It is a state machine and not two booleans: every call below states the phase it
   * needs, and one consumer — the subject bench — deliberately stops at `Prepared`. */
  enum class Phase { Declared, Prepared, Streaming };

  Outshine(const Scene &scene, const Assets &assets);

  /* THE ONLY TWO THINGS A CLIENT STILL SAYS. The tile server's address is where this machine
   * reaches the data and no property of the world; the stance is overridden only by a snapshot
   * another client wrote (Snapshot.h). Both are read by Prepare/Open, so they are refused
   * afterwards rather than silently ignored. */
  void SetTilesBase(const std::string &url) { if (Phase_ == Phase::Declared) Base_ = url; }
  void SetStance(const Stance &s) { if (Phase_ == Phase::Declared) Stance_ = s; }

  /* WHAT AN ANIMATION CHANNEL MAY MOVE PER FRAME (Animation.h). Each one ends in a renderer setter
   * that is safe to call inside a run; the quantities that are NOT here are the ones a mid-run
   * change would have to re-bake, and they are named in doc/clients/clients.md's Gaps. */
  void SetFovDeg(double deg);
  void SetExposureCompEv(double ev);
  void SetSkyOffsetS(double s);

  /* GIVE THE HOST ITS TURN. Natively nothing: the run owns the process. In the browser a headless
   * run holds the one thread the tile fetches complete on, so a run that never yielded would wait
   * for a world that can never arrive. */
  static void Pump();

  /* THE FPS SPECTRUM RIDES ALONG (FrameTelemetry.h). Frame() feeds it and emits one row per second,
   * so a client that only draws is observable without declaring a measurement. A null sink makes
   * the whole path a clock read. */
  void SetTelemetrySink(TelemetrySink *sink) { Bus_.SetSink(sink); }

  /* TWO PHASES, because one consumer stops between them: the subject bench (goal.md §3) judges a
   * plant with no world and no network at all, so it prepares and never opens. */
  bool Prepare(const Gpu &gpu);
  bool Open();
  bool Configure(const Gpu &gpu) { return Prepare(gpu) && Open(); }
  Phase Stage() const { return Phase_; }

  /* The wind clock is not the sky clock: the sun stands where the scene declared it while the flow
   * runs. It is the one quantity a sequence moves. */
  void SetWindClock(double s);

  /* Move the eye. The ground rides the DEM, so the height above sea level follows from wherever the
   * stance now is and a cold tile leaves the last resolved answer standing. */
  void Look(const Stance &s);

  /* One streaming pass. It decides nothing about images: the caller renders, or does not. */
  Progress Stream(double nowMs);
  void Frame();
  Counters Measured() const;

  /* THE TWO PEERS, borrowed. A bench reads back pixels, depth and counters and the browser reads
   * none of them, so narrowing this to the union of both callers would be forty forwarders that say
   * nothing — see doc/clients/clients.md's Gaps for the guideline this deviates from. */
  Render::Renderer &Renderer() { return R_; }
  World::World &Scenery() { return W_; }

  const World::Forest &Forest() const { return Forest_; }
  const World::Standpoint &Standpoint() const { return Stand_; }
  const World::VegetationTemplates &Vegetation() const { return Veg_; }
  const World::GroundMaterials &Materials() const { return Mats_; }
  const Scene &Declared() const { return Scene_; }
  const Assets &Files() const { return Assets_; }

  double Lat() const { return Stance_.Lat; }
  double Lon() const { return Stance_.Lon; }
  double YawDeg() const { return Stance_.YawDeg; }
  double PitchDeg() const { return Stance_.PitchDeg; }
  const double *Eye() const { return Eye_; }
  const double *Fwd() const { return Fwd_; }
  const double *Right() const { return Right_; }
  const double *Up() const { return Up_; }

  float SunElDeg() const { return SunEl_; }
  float SunAzDeg() const { return SunAz_; }
  double SkyClockS() const { return Clk_; }
  double WindDeg() const { return WindDeg_; }
  double WindMs() const { return WindMs_; }

private:
  bool ResolveGround(double lat, double lon, double *out) const;
  void CheckRoof();

  Scene Scene_;
  Assets Assets_;
  SceneWeather Wind_;
  Render::ExposureParams Exposure_;
  Stance Stance_;
  double WindDeg_, WindMs_, Clk_;

  World::GroundMaterials Mats_;
  World::VegetationTemplates Veg_;
  Render::Renderer R_;
  World::World W_;
  World::Forest Forest_;
  World::Standpoint Stand_;
  State State_;
  FrameTelemetry Frames_;
  TelemetryBus Bus_;
  double LastFrameMs_ = 0.0, ClockOriginMs_ = 0.0;

  std::string Base_ = "http://localhost:8081";
  double ViewM_ = 60000.0, OrthoM_ = 0.0;
  float SunEl_ = 0.0f, SunAz_ = 0.0f;
  double Eye_[3] = {}, Fwd_[3] = {}, Right_[3] = {}, Up_[3] = {};
  Phase Phase_ = Phase::Declared;
  bool RoofChecked_ = false;
};

} // namespace outshine::Clients
#endif
