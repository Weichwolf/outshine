/* FlightBox — FBGroundModule: the module a STATIC GROUND TARGET is. Structurally modules/stores'
 * FBStoreModule with one thing taken away rather than added: a released bomb has no pilot and no
 * guidance but it still integrates, and this has not even that. Its Run() is empty, because a bunker
 * does nothing, and it declares NO JSBSim model, which is the statement that makes it what it is.
 *
 * WHY NO AIRFRAME (the design question this class answers). Every other unit in this tree exists because
 * something flies it, and the FBSimUnit that owns it therefore owns an FBFdm. A ground target has no
 * flight dynamics at all, so the two ways to build one were: give it a trivial JSBSim model so nothing
 * else has to change, or let a unit exist without one. The first would have meant inventing an
 * aerodynamic object — mass, contact springs, a trim state — for a thing that does not move, and
 * integrating it at 100 Hz for the whole run to produce the position it was spawned at. So the airframe
 * is OPTIONAL at the unit level instead (units/FBSimUnit's banner): a unit with one is stepped, a unit
 * without one holds its declared pose, and everything else — identity, team, published pose, the health
 * register, the damage model, the mission roster, telemetry, the unit registry — is the same code for
 * both. FBModule::UnitKind is how a module says which it is, so no client has to know this class.
 *
 * ONE CLASS, N CATALOGUE ENTRIES, exactly as FBStoreModule: the target class it is comes from the
 * FBGroundTargetSpec handed to the constructor, and every entry of FBGroundTarget.h registers this same
 * class under its own name (FBGroundModuleRegistration).
 *
 * IT IS STILL A FULL FBModule. Every system slot is the airframe-agnostic default and none of them is
 * ever cycled — the slots exist because every module carries the same categories, and a target that
 * grows a radar one day fills one of them by derivation rather than by a new kind of object. */
#ifndef FBGROUNDMODULE_H
#define FBGROUNDMODULE_H

#include "FBGroundTarget.h"
#include "FBModule.h"
#include "FBSystemSlots.h"

namespace FlightBox {

class FBGroundModule : public FBModule {
public:
  explicit FBGroundModule(const FBGroundTargetSpec &spec) : Spec_(spec) { Rwr_.SetPowered(false); }

  const FBGroundTargetSpec &Spec() const { return Spec_; }

  /* No airframe, ever: an empty model name is what tells the spawn path there is nothing to load
   * (app/FBMissionBoot.h), and AttachFdm is consequently never called. */
  const char *FdmModelName() const override { return ""; }
  void AttachFdm(FBFdm &fdm) override { (void)fdm; }
  FBUnitKind UnitKind() const override { return FBUnitKind::Ground; }

  /* A target does nothing, on purpose. No FDM to step, no system to cycle, no state to advance — the
   * whole of its per-tick behaviour is that its pose is the one the mission declared. */
  void Run(fb_fdm_state &st, double dt, const FBUnitRegistry *units = nullptr,
           const FBWorld *world = nullptr) override {
    (void)st; (void)dt; (void)units; (void)world;
  }

  /* WHERE ITS SYSTEMS ARE and how much energy each takes — the module data core/FBDamageModel resolves
   * a burst against, exactly like modules/f16/FBF16Damage's for the jet. This is the one accessor that
   * makes a ground target a real participant rather than a marker. */
  const FBDamageLayout &DamageLayout() const override { return Spec_.Layout; }

  FBAutopilot &Autopilot() override { return AP_; }
  FBFlightControl &FlightControl() override { return FC_; }
  FBPilot &PilotSystem() override { return Pilot_; }
  FBAirframeControls &Controls() override { return Ctrl_; }
  FBDisplaySystem &Displays() override { return Disp_; }
  FBAirDataSystem &AirDataSystem() override { return AirData_; }
  FBNavSystem &NavSystem() override { return Nav_; }
  FBWarningSystem &WarningSystem() override { return Warn_; }
  FBRadarAltimeter &RadarAltimeter() override { return RadarAlt_; }
  FBCommandBus &Commands() override { return Cmds_; }
  FBDatalinkSystem &Datalink() override { return Datalink_; }
  FBRadarSystem &Radar() override { return Radar_; }
  FBRwrSystem &Rwr() override { return Rwr_; }
  FBCountermeasureSystem &Countermeasures() override { return Cm_; }
  FBStoresSystem &Stores() override { return Stores_; }
  FBGunSystem &Guns() override { return Gun_; }
  const FBState &Telemetry() const override { return State_; }
  const FBGuidance &LastGuidance() const override { return LastG_; }
  int LastSubsteps() const override { return 0; }

  FBFlightPlan &FlightPlan() override { return Plan_; }
  void SetRunway(const FBRunway &rwy) override { (void)rwy; }
  void SetGroundAsl(float m) override { (void)m; }
  /* A target takes no mission setup: WHAT it is, is its module name, and WHERE it is, is its spawn
   * line. Any `set` key is therefore unknown and voids the spawn — correctly, since a `set` aimed at a
   * target could only be a mission that thinks it is declaring an aircraft. */
  bool ApplySetup(const std::string &key, const std::string &value) override {
    (void)key; (void)value;
    return false;
  }

private:
  const FBGroundTargetSpec &Spec_;

  FBAutopilot AP_;
  FBFlightControl FC_;
  FBPilot Pilot_;
  FBAirframeControls Ctrl_;
  FBDisplaySystem Disp_;
  FBAirDataSystem AirData_;
  FBNavSystem Nav_;
  FBWarningSystem Warn_;
  FBRadarAltimeter RadarAlt_;
  FBCommandBus Cmds_;
  FBDatalinkSystem Datalink_;
  FBRadarSystem Radar_;
  FBRwrSystem Rwr_;
  FBCountermeasureSystem Cm_;
  FBStoresSystem Stores_;
  FBGunSystem Gun_;
  FBFlightPlan Plan_;
  FBState State_{};
  FBGuidance LastG_{};
};

} // namespace FlightBox
#endif
