/* FlightBox — FBF16Module: the F-16. Composes the systems/ DEFAULTS with the F-16 gain preset and
 * cycles every slot of the doc/modules/f16/ inventory, each at its own rate — the rate table with its
 * justifications is doc/modules/f16/module.md §2.2. This is where an F-16-specific override gets
 * hung, by replacing one slot's default with a subclass. */
#ifndef FBF16MODULE_H
#define FBF16MODULE_H

#include <memory>
#include "FBAirDataSystem.h"
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

  /* Every slot that observes other units needs its own identity: to skip its own PPLI/echo, to know
   * whose IFF crypto it holds, whose uplink a launched round listens to, and who fired a burst. */
  void SetUnitIdentity(int unitId, FBUnitTeam team) override {
    Datalink_->SetIdentity(unitId, team);
    Fcr_->SetIdentity(unitId, team);
    Rwr_->SetIdentity(unitId, team);
    SmsSys->SetUnitId(unitId);
    GunSys->SetUnitId(unitId);
  }

  FBMasterMode GetMasterMode() const { return Mode; }
  void SetMasterMode(FBMasterMode m) { Mode = m; }

  FBF16Pilot &PilotSystem() override { return *PilotSys; }   /* covariant: the base returns FBPilot& */
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

  const FBDamageLayout &DamageLayout() const override { return FBF16DamageLayout(); }

private:
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
  std::unique_ptr<FBF16Max7456> Chip;
  std::unique_ptr<FBF16Fcr> Fcr_;
  std::unique_ptr<Systems::FBWeaponSystem> Weapons;
  /* Defensive is TWO systems, cycled receiver-then-dispenser: the second reads what the first wrote. */
  std::unique_ptr<FBF16Rwr> Rwr_;
  Sensors::FBIrstSystem Irst_;
  std::unique_ptr<FBF16Cmds> Cmds_;
  std::unique_ptr<FBF16Datalink> Datalink_;

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
  FBMasterMode Mode = FBMasterMode::Nav;
  FBState SharedState{};   /* Sensors WRITE, Displays READ — no display queries a sensor directly */

  Systems::FBGuidance LastG{};
  double AccS = 0.0;
  int LastSub = 0;
  double DisplayAccS = 0.0, SensorAccS = 0.0, WeaponAccS = 0.0, DefensiveAccS = 0.0, CommsAccS = 0.0;
  /* Absolute sim seconds: sensors stamp and age their picture against this, so it cannot depend on how
   * often this module happens to cycle a slot. */
  double SimTimeS = 0.0;
};

} // namespace FlightBox::Modules
#endif
