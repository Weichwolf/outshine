/* THE SIMULATION HALF. One declared scene in, a world that answers questions out — and it never
 * draws: no device, no renderer, no camera basis leaves this object as anything but numbers. Both
 * targets link it; only the picture target puts a renderer over it (clients/Outshine.h). */
#ifndef SIM_H
#define SIM_H

#include <optional>
#include <string>

#include "Forest.h"
#include "GroundMaterials.h"
#include "Scene.h"
#include "SceneWeather.h"
#include "Standpoint.h"
#include "State.h"
#include "StreamTelemetry.h"
#include "Telemetry.h"
#include "VegetationTemplates.h"
#include "World.h"

namespace outshine::Clients {

class Sim {
public:
  /* The engine's own declarations, by path, because the two toolchains mount them differently — a
   * preloaded virtual FS in the browser, the working directory natively. Nothing here is content. */
  struct Assets {
    std::string Vegetation, GroundMaterials, Species, Moon;
  };
  struct Stance {
    double Lat = 0.0, Lon = 0.0, YawDeg = 0.0, PitchDeg = 0.0;
  };
  /* WHAT STANDS AT A PLACE, with no buffer existing and no device anywhere — the point query of
   * doc/architecture.md's product table. Metres are above the DEM's own datum. */
  struct Place {
    bool GroundResolved = false;
    double GroundAslM = 0.0;
    int Class = -1;                            /* vegetation table row; -1 = OSM has no datum here */
    double ClassEdgeM = 0.0;                   /* distance to that class's nearest edge */
    std::optional<double> StructureHeightM;    /* roof less ground */
    std::optional<double> WaterDepthM;         /* water level less ground */
  };

  Sim(const Scene &scene, const Assets &assets);

  /* THE ONLY TWO THINGS A CLIENT STILL SAYS. The tile server's address is where this machine
   * reaches the data and no property of the world; the stance is overridden only by a snapshot
   * another client wrote (Snapshot.h). Both are read by LoadTables/Open, so they are refused
   * afterwards rather than silently ignored. */
  void SetTilesBase(const std::string &url) { if (!Opened_) Base_ = url; }
  void SetStance(const Stance &s) { if (!Opened_) Stance_ = s; }

  /* The declared tables, before anything asks the world a question. */
  bool LoadTables();
  /* Opens the tile stream, resolves the ground under the standpoint and places sun and moon. Ends
   * with the eye standing where the scene declared it. */
  bool Open();
  bool Opened() const { return Opened_; }

  /* Grows the declared species. The yield is the picture's; the world keeps the crown and the
   * height spread it scatters with. Empty where no species is declared or it will not read. */
  std::optional<World::Forest::Prototype> GrowTrees();

  /* Move the eye. The ground rides the DEM, so the height above sea level follows from wherever the
   * stance now is and a cold tile leaves the last resolved answer standing. */
  void Look(const Stance &s);
  /* One simulation pass over the place the eye stands at. */
  void Advance();
  /* Once the vectors are in: the roof the eye may be inside, and the stand it may be inside. */
  void Settle();

  Place At(double lat, double lon) const;

  /* THE SKY CLOCK MOVES, the wind clock is a different one. `s` is an offset on the scene's own
   * instant, in seconds. */
  void SetSkyOffsetS(double s);

  /* THE IDENTITY SOURCE IS BORROWED and registered ahead of every other, so every row names its
   * run. Whoever owns a further source registers it on the same bus. */
  void SetTelemetrySink(TelemetrySink *sink) { Bus_.SetSink(sink); }
  void SetTelemetryIdentity(TelemetrySource *id) { Identity_ = id; }
  TelemetryBus &Bus() { return Bus_; }
  void StartTelemetry();
  StreamTelemetry &Streaming() { return Stream_; }
  const StreamTelemetry &Streaming() const { return Stream_; }

  World::World &Scenery() { return W_; }
  const World::World &Scenery() const { return W_; }
  const World::Forest &Forest() const { return Forest_; }
  const World::Standpoint &Standpoint() const { return Stand_; }
  const World::VegetationTemplates &Vegetation() const { return Veg_; }
  const World::GroundMaterials &Materials() const { return Mats_; }
  const Scene &Declared() const { return Scene_; }
  const Assets &Files() const { return Assets_; }
  const SceneWeather &Weather() const { return Wind_; }
  const State &SceneState() const { return State_; }
  const std::string &TilesBase() const { return Base_; }

  double Lat() const { return Stance_.Lat; }
  double Lon() const { return Stance_.Lon; }
  double YawDeg() const { return Stance_.YawDeg; }
  double PitchDeg() const { return Stance_.PitchDeg; }
  const Stance &Standing() const { return Stance_; }
  const double *Eye() const { return Eye_; }
  const double *Fwd() const { return Fwd_; }
  const double *Right() const { return Right_; }
  const double *Up() const { return Up_; }
  World::World::Eye Sight() const { return {Stance_.Lat, Stance_.Lon, Eye_, Fwd_}; }

  float SunElDeg() const { return SunEl_; }
  float SunAzDeg() const { return SunAz_; }
  double SkyClockS() const { return Clk_; }
  double WindDeg() const { return WindDeg_; }
  double WindMs() const { return WindMs_; }
  double ViewM() const { return ViewM_; }
  double OrthoM() const { return OrthoM_; }

  /* The ladder measures its screen-space error in pixels of the declared frame, so a field of view
   * that moves mid-run moves it too. */
  void SetPixelFocalLength(double px) { W_.SetPixelFocalLength(px); }

private:
  bool ResolveGround(double lat, double lon, double *out) const;

  Scene Scene_;
  Assets Assets_;
  SceneWeather Wind_;
  Stance Stance_;
  double WindDeg_, WindMs_, Clk_;

  World::GroundMaterials Mats_;
  World::VegetationTemplates Veg_;
  World::World W_;
  World::Forest Forest_;
  World::Standpoint Stand_;
  State State_;
  StreamTelemetry Stream_;
  TelemetrySource *Identity_ = nullptr;
  TelemetryBus Bus_;

  std::string Base_ = "http://localhost:8081";
  double ViewM_ = 60000.0, OrthoM_ = 0.0;
  float SunEl_ = 0.0f, SunAz_ = 0.0f;
  double Eye_[3] = {}, Fwd_[3] = {}, Right_[3] = {}, Up_[3] = {};
  bool Opened_ = false;
  bool RoofChecked_ = false;
};

} // namespace outshine::Clients
#endif
