/* FlightBox — FBF16Module: the F-16. Composes the systems/ DEFAULTS with the F-16 gain preset and
 * cycles every slot of the doc/modules/f16/ inventory, each at its own rate — the rate table with its
 * justifications is doc/modules/f16/module.md §2.2. This is where an F-16-specific override gets
 * hung, by replacing one slot's default with a subclass. */
#ifndef FBF16MODULE_H
#define FBF16MODULE_H

#include <memory>
#include "FBAirDataSystem.h"
#include "FBAirNet.h"
#include "FBAirframeControls.h"
#include "FBAutopilot.h"
#include "FBF16Cmds.h"
#include "FBF16Damage.h"
#include "FBF16Datalink.h"
#include "FBF16Fcr.h"
#include "FBF16FireControl.h"
#include "FBF16Gun.h"
#include "FBF16Max7456.h"
#include "FBF16Pilot.h"
#include "FBF16Rwr.h"
#include "FBF16Sms.h"
#include "FBF16Ufc.h"
#include "FBFlightControl.h"
#include "FBFlightPlan.h"
#include "FBMfdSystem.h"
#include "FBModule.h"
#include "FBNavSystem.h"
#include "FBPilot.h"
#include "FBRadarAltimeter.h"
#include "FBRunway.h"
#include "FBSystemSlots.h"
#include "FBWarningSystem.h"
#include "FBMasterMode.h"

namespace FlightBox::Modules {

class FBF16Module : public FBModule {
public:
  FBF16Module();

  /* Also swaps the NoOp airframe-controls default for the real, FDM-bound one. */
  void AttachFdm(Fdm::FBFdm &fdm) override;

  /* Deliberately NOT derived from the registry name: the two coincide today, they are not one thing. */
  const char *FdmModelName() const override { return "f16"; }

  void Run(Fdm::fb_fdm_state &st, double dt, const Units::FBUnitRegistry *units = nullptr,
           const World::FBWorld *world = nullptr) override;

  /* The HUD's telemetry chain writes HERE, so a client seeds its per-frame FBState from this BEFORE
   * overwriting the fields it computes itself. */
  const FBState &Telemetry() const override { return SharedState; }

  Systems::FBAutopilot &Autopilot() override { return *AP; }
  Systems::FBFlightControl &FlightControl() override { return *FC; }
  Systems::FBDisplaySystem &Displays() override { return *Disp; }
  FBF16Max7456 &Max7456() { return *Chip; }
  Systems::FBNavSystem &NavSystem() override { return *NavSys; }
  Sensors::FBDatalinkSystem &Datalink() override { return *Datalink_; }
  FBF16Fcr &Radar() override { return *Fcr_; }   /* covariant: the base returns FBRadarSystem& */
  FBF16Rwr &Rwr() override { return *Rwr_; }
  /* The F-16 has no IRST. The slot holds the generic default, is never cycled and never powered, so
   * its block stays Invalid — a module declares what it HAS, and the alternative (leaving the accessor
   * out) would mean a caller holding an FBModule& could not ask. */
  Sensors::FBIrstSystem &Irst() override { return Irst_; }
  /* ...and the one sensor no airframe carries and every pilot has. It is composed like any other slot
   * because it must sit INSIDE the perception boundary (doc/sensors.md §9.1). */
  Sensors::FBVisualSystem &Visual() override { return Visual_; }
  FBF16Cmds &Countermeasures() override { return *Cmds_; }
  FBF16Ufc &Ufc() { return *UfcSys; }
  FBF16Sms &Sms() { return *SmsSys; }
  Weapons::FBStoresSystem &Stores() override { return *SmsSys; }
  FBF16Gun &Gun() { return *GunSys; }
  Weapons::FBGunSystem &Guns() override { return *GunSys; }
  const Systems::FBGuidance &LastGuidance() const override { return LastG; }
  int LastSubsteps() const override { return LastSub; }

  /* The SAME DEM sample the client already resolved for the renderer — FBRadarAltimeter reuses it
   * rather than re-querying terrain. */
  void SetGroundAsl(float m) override { GroundAslM = m; }

  /* The cloud decks over this jet: the EYE marches them for a line-of-sight transmittance (this
   * airframe has no IRST to hand them to). Never called = a clear sky. */
  void SetCloudSky(const FBCloudSky &sky) override { Visual_.SetSky(sky); }

  /* Onto the bus, not into a member: FBEnvironmentBlock is where everything that reads daylight looks,
   * and the block's status is what says a clock was declared at all. */
  void SetSolar(const FBSolar &solar) override { FBSolarToEnv(solar, SharedState); }

  /* Every slot that observes other units needs its own identity: to skip its own PPLI/echo, to know
   * whose IFF crypto it holds, whose uplink a launched round listens to, and who fired a burst. */
  void SetUnitIdentity(int unitId, FBUnitTeam team) override {
    Datalink_->SetIdentity(unitId, team);
    Fcr_->SetIdentity(unitId, team);
    Rwr_->SetIdentity(unitId, team);
    SmsSys->SetUnitId(unitId);
    GunSys->SetUnitId(unitId);
    Visual_.SetIdentity(unitId);   /* the id only — an eye is given no team to read */
  }

  FBMasterMode GetMasterMode() const { return Mode; }
  void SetMasterMode(FBMasterMode m) { Mode = m; }

  FBF16Pilot &PilotSystem() override { return *PilotSys; }   /* covariant: the base returns FBPilot& */
  Systems::FBInputSystem *HumanInput() override { return Input.get(); }
  Systems::FBAirframeControls &Controls() override { return *AirframeCtrl; }
  Systems::FBAirDataSystem &AirDataSystem() override { return *AirData; }
  Systems::FBWarningSystem &WarningSystem() override { return *Warn_; }
  FBCommandBus &Commands() override { return CmdBus_; }
  Systems::FBRadarAltimeter &RadarAltimeter() override { return *RadarAlt; }
  FBFlightPlan &FlightPlan() override { return Plan_; }
  /* The runway doubles as the mission's BULLSEYE: a .fbm declares none, and the runway is the one
   * briefed point every unit shares. Without one the HUD's bearing/range pair stays at the origin. */
  void SetRunway(const FBRunway &rwy) override {
    Rwy_ = rwy; HaveRunway_ = true;
    NavSys->SetBullseye(rwy.ThresholdLatDeg, rwy.ThresholdLonDeg);
  }

  /* The key set is doc/missions/INDEX.md + doc/modules/f16/module.md §2.6. An unknown key OR an
   * unparsable/out-of-range value returns false (mission FAIL): a mission that declares a state this
   * airframe cannot take must not start silently in some other state. */
  bool ApplySetup(const std::string &key, const std::string &value) override;

  /* A POINT, AN AGE AND NO IDENTITY, published only while this jet is on a declared net. On a fighter
   * the surveillance report rides the SAME Link-16 terminal the flight's PPLI does. modules/FBAirNet.h. */
  FBNetReport NetReport() override { return Node_; }
  const char *NetControlNode() const override { return Net_.Control; }

  /* [SET, a CLASS not a measurement] ~1.2 m^2, the figure most often quoted for a clean F-16 without
   * the later low-observable treatments (T4-grade: no primary source publishes one). It is also the
   * REFERENCE of the radar equation's fourth-root scaling (sensors/FBRadarSystem::kRefRcsM2) — which
   * is the deliberate calibration choice, not a coincidence: every radar range in this tree was
   * measured against this aircraft, so declaring its own cross-section as the reference makes the
   * whole cross-section mechanism the identity for F-16 against F-16 and an asymmetry only across
   * types. doc/sensors.md, Spec. */
  double RadarCrossSectionM2() const override { return 1.2; }

  const FBDamageLayout &DamageLayout() const override { return FBF16DamageLayout(); }

private:
  static Pilot::FBPilotCommands HandsToCommands(const Systems::FBStickInput &s);
  void ApplyPilotCommands(const Pilot::FBPilotCommands &c);
  void PublishPlatform(const Fdm::fb_fdm_state &st);
  void PublishAirframe();
  /* Hands every DUE command of one group to the system that owns it, in that system's own slot tick,
   * and answers with the outcome the box itself decided. */
  void ServiceCommands(FBCommandGroup group);
  void ApplyCommand(const FBAvionicsCommand &c, FBCommandOutcome &outcome, FBCommandReason &reason);
  static bool Due(double &accS, double dt, double hz);

  /* Base pointers, not value members, so a future module can substitute an override without slicing. */
  std::unique_ptr<Systems::FBAutopilot> AP;
  std::unique_ptr<Systems::FBFlightControl> FC;

  std::unique_ptr<Systems::FBInputSystem> Input;
  std::unique_ptr<Systems::FBPropulsionSystem> Propulsion;
  std::unique_ptr<Systems::FBDisplaySystem> Disp;
  /* DER SEITENKATALOG DIESES COCKPITS und sonst nichts Modulspezifisches: die Bank selbst ist generisch.
   * Die F-16 hat den kooperativen Datenlink (HSD) und KEINEN IRST — ihr Modul weist `IrstMode` schon
   * als NotImplemented ab, also darf hier auch keine IRST-Seite stehen. doc/modules/f16/cockpit-displays.md. */
  Systems::FBMfdSystem Mfd_;
  std::unique_ptr<FBF16Max7456> Chip;
  std::unique_ptr<FBF16Fcr> Fcr_;
  std::unique_ptr<Systems::FBWeaponSystem> Weapons;
  /* Defensive is TWO systems, cycled receiver-then-dispenser: the second reads what the first wrote. */
  std::unique_ptr<FBF16Rwr> Rwr_;
  Sensors::FBIrstSystem Irst_;
  Sensors::FBVisualSystem Visual_;
  std::unique_ptr<FBF16Cmds> Cmds_;
  std::unique_ptr<FBF16Datalink> Datalink_;
  /* The mission `net` block, if one put this jet in it. Empty = on no net, and then every line of
   * modules/FBAirNet.h is inert — which is every mission written before this. */
  FBAirNet Net_{};

  /* The HUD's telemetry chain: generic systems/ defaults + the F-16-specific boxes. */
  std::unique_ptr<Systems::FBAirDataSystem> AirData;
  std::unique_ptr<Systems::FBRadarAltimeter> RadarAlt;
  std::unique_ptr<Systems::FBNavSystem> NavSys;
  std::unique_ptr<FBF16FireControl> FireCtrl;
  std::unique_ptr<FBF16Ufc> UfcSys;
  std::unique_ptr<FBF16Sms> SmsSys;
  std::unique_ptr<FBF16Gun> GunSys;
  std::unique_ptr<Systems::FBWarningSystem> Warn_;
  float GroundAslM = 0.0f;

  std::unique_ptr<FBF16Pilot> PilotSys;
  std::unique_ptr<Systems::FBAirframeControls> AirframeCtrl;   /* NoOp until AttachFdm, FDM-bound after */
  Fdm::FBFdm *Fdm_ = nullptr;                              /* borrowed, never owned */
  FBFlightPlan Plan_;
  FBRunway Rwy_;
  bool HaveRunway_ = false;
  double PilotAccS = 0.0;

  FBCommandBus CmdBus_;   /* the pilot's only path to this jet's boxes */
  FBNetReport Node_{};
  FBMasterMode Mode = FBMasterMode::Nav;
  FBState SharedState{};   /* Sensors WRITE, Displays READ — no display queries a sensor directly */

  Systems::FBGuidance LastG{};
  /* The last FLCS command, kept beside the guidance it came from and for one further reason: it is
   * what the throttle was doing when a pair of human hands took the jet over (FBInputSystem::Seed). */
  Systems::FBControls LastCtl{};
  double AccS = 0.0;
  int LastSub = 0;
  double DisplayAccS = 0.0, SensorAccS = 0.0, WeaponAccS = 0.0, DefensiveAccS = 0.0, CommsAccS = 0.0;
  /* Absolute sim seconds: sensors stamp and age their picture against this, so it cannot depend on how
   * often this module happens to cycle a slot. */
  double SimTimeS = 0.0;
  /* WHAT AN ANTI-RADIATION ROUND ON THIS JET IS PROGRAMMED AGAINST (`set arm_class`). It lives here and
   * not in the SMS because it is a classification and the SMS classifies nothing.
   * doc/air-to-ground.md §8. */
  FBArTargetClass ArClass_ = FBArTargetClass::AnySurface;
};

} // namespace FlightBox::Modules
#endif
