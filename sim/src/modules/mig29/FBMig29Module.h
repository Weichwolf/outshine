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
#include "FBMig29Cmds.h"
#include "FBMig29Damage.h"
#include "FBMig29FireControl.h"
#include "FBMig29Gun.h"
#include "FBMfdSystem.h"
#include "FBMig29Irst.h"
#include "FBMig29Pilot.h"
#include "FBMig29Radar.h"
#include "FBMig29Sms.h"
#include "FBMig29Rwr.h"
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
  FBMig29Radar &Radar() override { return Radar_; }        /* covariant, like the F-16's */
  FBMig29Rwr &Rwr() override { return Rwr_; }
  FBMig29Irst &Irst() override { return Irst_; }
  Sensors::FBVisualSystem &Visual() override { return Visual_; }   /* generic: an eye is not a box */
  FBMig29Cmds &Countermeasures() override { return Cm_; }   /* covariant, like the F-16's */
  FBMig29Sms &Stores() override { return Stores_; }        /* covariant: pylon geometry only */
  FBMig29Gun &Guns() override { return Gun_; }             /* covariant: installation only */
  const Systems::FBGuidance &LastGuidance() const override { return LastG; }
  int LastSubsteps() const override { return LastSub; }

  void SetGroundAsl(float m) override { GroundAslM = m; }
  /* The one consumer of weather above the FDM in this tree: the optical head, which cannot see through
   * a deck. Handed straight down — the module keeps no copy it could let go stale. */
  void SetCloudSky(const FBCloudSky &sky) override { Irst_.SetSky(sky); Visual_.SetSky(sky); }
  void SetSolar(const FBSolar &solar) override { FBSolarToEnv(solar, SharedState); }

  /* Who this aircraft IS, for the slots that observe other units — and, for the SMS, so a round it
   * launches knows whose illumination to listen for. */
  void SetUnitIdentity(int unitId, FBUnitTeam team) override {
    Radar_.SetIdentity(unitId, team);
    Rwr_.SetIdentity(unitId, team);
    Irst_.SetIdentity(unitId, team);
    Visual_.SetIdentity(unitId);   /* the id only — an eye is given no team to read */
    Datalink_.SetIdentity(unitId, team);
    Stores_.SetUnitId(unitId);
    /* And the GUN, for the same reason plus a sharper one: the owner of the simulation excludes the
     * SHOOTER from a burst's hit resolution by comparing the bundle's LauncherId, so a gun that does
     * not know who fired it shoots its own aircraft down at the muzzle (measured, before this line). */
    Gun_.SetUnitId(unitId);
  }

  /* [SET, a CLASS not a measurement] The radar cross-section this airframe presents, ~4 m^2 — the
   * middle of the 3-5 m^2 band commonly quoted for an unmodified 9-12 (T4-grade: no primary source
   * publishes one, and a two-decimal figure would be false precision). It is 3.3x the F-16's declared
   * 1.2 m^2, which through the fourth root of the radar equation is 1.35x the detection range in one
   * direction and 0.74x in the other — the asymmetry this stage exists to produce, and the whole of
   * it. doc/sensors.md, Spec. */
  double RadarCrossSectionM2() const override { return 4.0; }

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
  /* The MODULE routes, for the same reason it interprets its own `set` keys: the systems are generic,
   * and which box on THIS panel owns "radar mode" is this aircraft's knowledge. */
  void ServiceCommands(FBCommandGroup group, const Fdm::fb_fdm_state &st);
  /* `st` travels down because ONE command on this panel is answered against the airframe's own pose:
   * the LOCKON/trigger pair of the air-to-ground director, which ranges and plans from where the jet
   * is at the instant the hand moved. Every other target ignores it. */
  void ApplyCommand(const FBAvionicsCommand &c, const Fdm::fb_fdm_state &st,
                    FBCommandOutcome &outcome, FBCommandReason &reason);
  void PublishPlatform(const Fdm::fb_fdm_state &st);
  void PublishAirframe();
  /* Flaps + slats as ONE command, because the deck couples them ([DCS-EA p.57]: either DOWN button
   * extends both). Driven by the pilot's own phase — see the definition. */
  void RunConfiguration();
  static bool Due(double &accS, double dt, double hz);

  std::unique_ptr<Systems::FBAutopilot> AP;
  std::unique_ptr<Systems::FBFlightControl> FC;
  std::unique_ptr<Systems::FBDisplaySystem> Disp;
  /* DER KATALOG DIESES COCKPITS. Die MiG-29 hat den passiven OEPS-29/KOLS (IRST-Seite) und den
   * SPO-15 — und KEINE kooperative Lageseite: ihr Datenlink ist ein Kommandokanal vom Boden, keine
   * Verbandslage. Was fehlt, ist als Luecke aufgeschrieben statt der F-16 entliehen. */
  Systems::FBMfdSystem Mfd_;
  std::unique_ptr<Systems::FBAirDataSystem> AirData;
  std::unique_ptr<Systems::FBRadarAltimeter> RadarAlt;
  std::unique_ptr<Systems::FBNavSystem> NavSys;
  std::unique_ptr<Systems::FBWarningSystem> Warn_;
  std::unique_ptr<FBMig29Pilot> PilotSys;
  std::unique_ptr<Systems::FBAirframeControls> AirframeCtrl;   /* NoOp until AttachFdm, FDM-bound after */

  /* THE THREE SENSORS OF STAGE 2b, each a real derivation with its own numbers. */
  FBMig29Radar Radar_;   /* N019 "Rubin" — the ACTIVE one, and the one that gives the jet away */
  FBMig29Rwr Rwr_;       /* SPO-15LM "Beryoza" — the PASSIVE warning half */
  FBMig29Irst Irst_;     /* OEPS-29 / KOLS — the PASSIVE optical one, this aircraft's own asymmetry */
  Sensors::FBVisualSystem Visual_;
  /* Still absent: the cooperative terminal this jet does not have at all — its GCI link is a VOICE
   * channel the pilot types in (doc/modules/mig29/datalink-gci.md), not a track picture. Never cycled,
   * block stays Invalid. */
  Sensors::FBDatalinkSystem Datalink_;
  /* THE BVP-30-26, the passive dispensers — the piece that makes the defensive asymmetry two-sided. */
  FBMig29Cmds Cm_;
  /* STAGE 2c: the weapons. The three slots are real from here on — pylon geometry, gun installation and
   * this aircraft's own weapon-control computer; all the BEHAVIOUR is the generic weapons/ code. */
  FBMig29Sms Stores_;
  FBMig29Gun Gun_;
  std::unique_ptr<FBMig29FireControl> FireCtrl_;

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
  double PilotAccS = 0.0, SensorAccS = 0.0, DisplayAccS = 0.0, DefensiveAccS = 0.0;
  double SimTimeS = 0.0;
};

} // namespace FlightBox::Modules
#endif
