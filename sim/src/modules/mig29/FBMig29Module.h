/* FlightBox — FBMig29Module: the MiG-29 9-12, and the proof that the module architecture carries a
 * SECOND airframe. It composes the systems/ DEFAULTS unchanged and adds exactly three things of its
 * own: this aircraft's pilot numbers (FBMig29Pilot), this aircraft's FBW gain preset
 * (FBFlightControl::Mig29()) and this aircraft's damage zones (FBMig29Damage).
 *
 * WHAT IS DELIBERATELY ABSENT (stage 2b/2c, doc/flightbox/aircraft/mig29.md gaps 2/6/7): radar, RWR,
 * IRST, countermeasures, stores, gun, datalink, GCI doctrine. Their slots hold the NoOp/generic
 * defaults, they are NOT cycled, and their blocks stay Invalid — a module declares what it HAS, and an
 * N019 that publishes empty contacts would be a sensor the pilot could believe. The F-16's per-slot
 * damage gating is therefore absent too: there is nothing here to gate.
 *
 * The one behaviour beyond composition is the CONFIGURATION SCHEDULE (flaps/slats), and it is here
 * rather than in pilot/FBPilot because systems/FBAirframeControls carries no flap channel and this is
 * the only airframe in the tree that has manual flaps at all — see Run(). */
#ifndef FBMIG29MODULE_H
#define FBMIG29MODULE_H

#include <memory>
#include "FBAirDataSystem.h"
#include "FBAirframeControls.h"
#include "FBAutopilot.h"
#include "FBFlightControl.h"
#include "FBFlightPlan.h"
#include "FBMasterMode.h"
#include "FBMig29Damage.h"
#include "FBMig29Pilot.h"
#include "FBModule.h"
#include "FBNavSystem.h"
#include "FBRadarAltimeter.h"
#include "FBRunway.h"
#include "FBSystemSlots.h"
#include "FBWarningSystem.h"

namespace FlightBox::Modules {

class FBMig29Module : public FBModule {
public:
  FBMig29Module();

  void AttachFdm(Fdm::FBFdm &fdm) override;

  const char *FdmModelName() const override { return "mig29"; }

  void Run(Fdm::fb_fdm_state &st, double dt, const Units::FBUnitRegistry *units = nullptr,
           const World::FBWorld *world = nullptr) override;

  const FBState &Telemetry() const override { return SharedState; }

  Systems::FBAutopilot &Autopilot() override { return *AP; }
  Systems::FBFlightControl &FlightControl() override { return *FC; }
  FBMig29Pilot &PilotSystem() override { return *PilotSys; }   /* covariant, like the F-16's */
  Systems::FBAirframeControls &Controls() override { return *AirframeCtrl; }
  Systems::FBDisplaySystem &Displays() override { return *Disp; }
  Systems::FBAirDataSystem &AirDataSystem() override { return *AirData; }
  Systems::FBNavSystem &NavSystem() override { return *NavSys; }
  Systems::FBWarningSystem &WarningSystem() override { return *Warn_; }
  Systems::FBRadarAltimeter &RadarAltimeter() override { return *RadarAlt; }
  FBCommandBus &Commands() override { return CmdBus_; }
  Sensors::FBDatalinkSystem &Datalink() override { return Datalink_; }
  Sensors::FBRadarSystem &Radar() override { return Radar_; }
  Sensors::FBRwrSystem &Rwr() override { return Rwr_; }
  Sensors::FBCountermeasureSystem &Countermeasures() override { return Cm_; }
  Weapons::FBStoresSystem &Stores() override { return Stores_; }
  Weapons::FBGunSystem &Guns() override { return Gun_; }
  const Systems::FBGuidance &LastGuidance() const override { return LastG; }
  int LastSubsteps() const override { return LastSub; }

  void SetGroundAsl(float m) override { GroundAslM = m; }

  FBFlightPlan &FlightPlan() override { return Plan_; }
  /* The runway doubles as the mission's bullseye, exactly as on the F-16 — one briefed point per
   * mission, and the generic Nav slot has nowhere else to get one. */
  void SetRunway(const FBRunway &rwy) override {
    Rwy_ = rwy; HaveRunway_ = true;
    NavSys->SetBullseye(rwy.ThresholdLatDeg, rwy.ThresholdLonDeg);
  }

  bool ApplySetup(const std::string &key, const std::string &value) override;

  const FBDamageLayout &DamageLayout() const override { return FBMig29DamageLayout(); }

private:
  void ApplyPilotCommands(const Pilot::FBPilotCommands &c);
  void PublishPlatform(const Fdm::fb_fdm_state &st);
  void PublishAirframe();
  /* Flaps + slats as ONE command, because the deck couples them ([DCS-EA p.57]: either DOWN button
   * extends both). Driven by the pilot's own phase — see the definition. */
  void RunConfiguration();
  static bool Due(double &accS, double dt, double hz);

  std::unique_ptr<Systems::FBAutopilot> AP;
  std::unique_ptr<Systems::FBFlightControl> FC;
  std::unique_ptr<Systems::FBDisplaySystem> Disp;
  std::unique_ptr<Systems::FBAirDataSystem> AirData;
  std::unique_ptr<Systems::FBRadarAltimeter> RadarAlt;
  std::unique_ptr<Systems::FBNavSystem> NavSys;
  std::unique_ptr<Systems::FBWarningSystem> Warn_;
  std::unique_ptr<FBMig29Pilot> PilotSys;
  std::unique_ptr<Systems::FBAirframeControls> AirframeCtrl;   /* NoOp until AttachFdm, FDM-bound after */

  /* The slots this airframe does not model yet. Value members and never cycled: they exist so that a
   * caller holding an FBModule& finds every category, and stage 2b fills one by replacing a member. */
  Sensors::FBDatalinkSystem Datalink_;
  Sensors::FBRadarSystem Radar_;
  Sensors::FBRwrSystem Rwr_;
  Sensors::FBCountermeasureSystem Cm_;
  Weapons::FBStoresSystem Stores_;
  Weapons::FBGunSystem Gun_;

  Fdm::FBFdm *Fdm_ = nullptr;   /* borrowed, never owned */
  FBFlightPlan Plan_;
  FBRunway Rwy_;
  bool HaveRunway_ = false;
  float GroundAslM = 0.0f;
  /* [DOC] doc/mig29/procedures.md §1: "flap retraction at 350 ft" [DCS-EA p.77]. */
  static constexpr double kFlapRetractAglFt = 350.0;
  double FlapCmd_ = -1.0;       /* last commanded position; < 0 = never commanded, so nothing is written */
  bool FlapRetracted_ = false;  /* latched at 350 ft, cleared by the next ground or approach phase */

  FBCommandBus CmdBus_;
  FBMasterMode Mode = FBMasterMode::Nav;
  FBState SharedState{};

  Systems::FBGuidance LastG{};
  double AccS = 0.0;
  int LastSub = 0;
  double PilotAccS = 0.0, SensorAccS = 0.0, DisplayAccS = 0.0;
  double SimTimeS = 0.0;
};

} // namespace FlightBox::Modules
#endif
