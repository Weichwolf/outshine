/* FlightBox — FBGroundModule: a STATIC GROUND TARGET. Structurally FBStoreModule MINUS one thing rather
 * than plus one — a released bomb has no pilot and no guidance but still integrates; this has not even
 * that. It is still a full FBModule: the slots exist because every module carries the same categories,
 * so a target that grows a radar one day fills one by derivation and not by a new kind of object.
 * Why it has NO AIRFRAME at all: doc/flightbox/weapons-and-damage.md §10.3. */
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

  /* The EMPTY model name is the signal to the spawn path that there is nothing to load, so AttachFdm
   * is never called. */
  const char *FdmModelName() const override { return ""; }
  void AttachFdm(FBFdm &fdm) override { (void)fdm; }
  FBUnitKind UnitKind() const override { return FBUnitKind::Ground; }

  /* Empty on purpose: the whole per-tick behaviour of a target is that its pose is the declared one. */
  void Run(fb_fdm_state &st, double dt, const FBUnitRegistry *units = nullptr,
           const FBWorld *world = nullptr) override {
    (void)st; (void)dt; (void)units; (void)world;
  }

  /* The ONE non-default accessor, and what makes a target a real participant rather than a marker. */
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
  /* WHAT a target is, is its module name; WHERE it is, is its spawn line. Any `set` key is unknown. */
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
