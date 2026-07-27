/* FlightBox — FBMissileModule: the module a GUIDED weapon flies with, and the exact counterpart of
 * modules/stores/FBStoreModule. A bomb's module has no pilot and no sensors, so its Run() only
 * integrates; a missile has both, so its Run() cycles them — and that is the whole difference. It is a
 * full FBModule over one JSBSim model (sim/assets/aircraft/aim120, FlightBox's own — the pinned
 * submodule carries no AMRAAM and is read-only), owned by one units/FBSimUnit, stepped by the same
 * runner and judged by the same monitors as every jet in the mission.
 *
 * ONE CLASS, N CATALOGUE ENTRIES, exactly like FBStoreModule: the round it flies is the FBStoreSpec
 * handed to the constructor, so every Guided entry of core/FBStore.h registers this same class under its
 * own name (FBMissileModuleRegistration).
 *
 * THE THREE SLOTS IT ACTUALLY FILLS, each a real system in its own right rather than a special case:
 *   Sensors  FBMissileSeeker  — the active radar seeker (a systems/FBRadarSystem with a narrow, slaved,
 *                               staring volume). The ONLY thing in this module that sees the registry.
 *   Comms    FBMissileUplink  — the midcourse receiver (a systems/FBDatalinkSystem that listens for its
 *                               launcher's radiated guidance uplink).
 *   Pilot    FBMissileGuidance— proportional navigation + the round's lateral-acceleration autopilot,
 *                               producing FIN commands.
 * Everything else is the airframe-agnostic default: guidance/FCS pass the fin commands through in
 * Manual (systems/FBAutopilot, systems/FBFlightControl), and there are no displays, no nav, no warning
 * set and no stores of its own.
 *
 * RATES. The seeker and the guidance run INSIDE the 100 Hz substep loop, next to the FCS: a round that
 * closes at 1.5 km/s covers 15 m per 10 ms, so a 10 Hz decision tick — right for a pilot — would be a
 * 150 m guidance quantum. The seeker's antenna keeps its own absolute-time frame grid (0.05 s), so it is
 * entered often but only LOOKS at its own frame rate. The uplink receiver runs once per Run(): the
 * launcher's fire control cannot produce a fresher estimate than its own radar frame anyway. */
#ifndef FBMISSILEMODULE_H
#define FBMISSILEMODULE_H

#include "FBMissileGuidance.h"
#include "FBMissileSeeker.h"
#include "FBMissileUplink.h"
#include "FBModule.h"
#include "FBStore.h"
#include "FBSystemSlots.h"

namespace FlightBox {

class FBMissileModule : public FBModule {
public:
  explicit FBMissileModule(const FBStoreSpec &spec);

  const FBStoreSpec &Spec() const { return Spec_; }

  void AttachFdm(FBFdm &fdm) override { Fdm_ = &fdm; }
  const char *FdmModelName() const override { return Spec_.FdmModel; }
  bool FdmModelVendored() const override { return Spec_.Vendored; }

  /* The LAUNCH PROGRAMMING, applied once by the spawn path (app/FBMissionBoot.h) before the first tick:
   * the shooter's target estimate, whose uplink to listen to, and the mission time of the release —
   * which seeds this module's clock, so the round's notion of "now" is the same one every other unit
   * (and every message age) is stamped against. */
  void ProgramRelease(const FBStoreRelease &rel) override;

  void Run(fb_fdm_state &st, double dt, const FBUnitRegistry *units = nullptr,
           const FBWorld *world = nullptr) override;

  FBAutopilot &Autopilot() override { return AP_; }
  FBFlightControl &FlightControl() override { return FC_; }
  FBMissileGuidance &PilotSystem() override { return Guidance_; }   /* covariant: FBPilot& on the base */
  FBAirframeControls &Controls() override { return Ctrl_; }
  FBDisplaySystem &Displays() override { return Disp_; }
  FBAirDataSystem &AirDataSystem() override { return AirData_; }
  FBNavSystem &NavSystem() override { return Nav_; }
  FBWarningSystem &WarningSystem() override { return Warn_; }
  FBRadarAltimeter &RadarAltimeter() override { return RadarAlt_; }
  FBCommandBus &Commands() override { return Cmds_; }
  FBMissileUplink &Datalink() override { return Uplink_; }          /* covariant */
  FBMissileSeeker &Radar() override { return Seeker_; }             /* covariant */
  /* No warning receiver and no dispenser on a store: the slots exist because every module carries the
   * same categories (FBModule's banner), and these two are the airframe-agnostic defaults, never
   * cycled. Powered down at construction so nothing they hold can be mistaken for a picture. */
  FBRwrSystem &Rwr() override { return Rwr_; }
  FBCountermeasureSystem &Countermeasures() override { return Cm_; }
  FBStoresSystem &Stores() override { return Stores_; }             /* a round carries no stores */
  const FBState &Telemetry() const override { return State_; }
  const FBGuidance &LastGuidance() const override { return LastG_; }
  int LastSubsteps() const override { return LastSub_; }

  FBFlightPlan &FlightPlan() override { return Plan_; }
  void SetRunway(const FBRunway &rwy) override { (void)rwy; }
  void SetGroundAsl(float m) override { GroundAslM_ = m; }
  /* A launched round takes no mission setup for the same reason a released bomb does not: it was
   * configured by being loaded and then programmed at launch. Any `set` key is therefore unknown, which
   * the caller turns into a mission FAIL. */
  bool ApplySetup(const std::string &key, const std::string &value) override {
    (void)key; (void)value;
    return false;
  }

private:
  const FBStoreSpec &Spec_;
  FBFdm *Fdm_ = nullptr;          /* borrowed, never owned (AttachFdm) */
  float GroundAslM_ = 0.0f;
  double AccS_ = 0.0;
  double SimTimeS_ = 0.0;         /* seeded with the release time — see Program() */
  int LastSub_ = 0;

  FBAutopilot AP_;
  FBFlightControl FC_;
  FBMissileGuidance Guidance_;
  FBMissileSeeker Seeker_;
  FBRwrSystem Rwr_;
  FBCountermeasureSystem Cm_;
  FBMissileUplink Uplink_;
  FBAirframeControls Ctrl_;
  FBDisplaySystem Disp_;
  FBAirDataSystem AirData_;
  FBNavSystem Nav_;
  FBWarningSystem Warn_;
  FBRadarAltimeter RadarAlt_;
  FBCommandBus Cmds_;
  FBStoresSystem Stores_;
  FBFlightPlan Plan_;
  FBState State_{};
  FBGuidance LastG_{};
};

} // namespace FlightBox
#endif
