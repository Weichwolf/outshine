/* FlightBox — FBGroundModule: a STATIC GROUND TARGET. Structurally FBStoreModule MINUS one thing rather
 * than plus one — a released bomb has no pilot and no guidance but still integrates; this has not even
 * that. It is still a full FBModule: the slots exist because every module carries the same categories,
 * so a target that grows a radar one day fills one by derivation and not by a new kind of object.
 * Why it has NO AIRFRAME at all: doc/weapons-and-damage.md §10.3. */
#ifndef FBGROUNDMODULE_H
#define FBGROUNDMODULE_H

#include "FBGroundTarget.h"
#include "FBModule.h"
#include "FBSystemSlots.h"

namespace FlightBox::Modules {

class FBGroundModule : public FBModule {
public:
  explicit FBGroundModule(const FBGroundTargetSpec &spec) : Spec_(spec) { Rwr_.SetPowered(false); }

  const FBGroundTargetSpec &Spec() const { return Spec_; }

  /* The EMPTY model name is the signal to the spawn path that there is nothing to load, so AttachFdm
   * is never called. */
  const char *FdmModelName() const override { return ""; }
  void AttachFdm(Fdm::FBFdm &fdm) override { (void)fdm; }
  Units::FBUnitKind UnitKind() const override { return Units::FBUnitKind::Ground; }

  /* Empty on purpose: the whole per-tick behaviour of a target is that its pose is the declared one. */
  void Run(Fdm::fb_fdm_state &st, double dt, const Units::FBUnitRegistry *units = nullptr,
           const World::FBWorld *world = nullptr) override {
    (void)st; (void)dt; (void)units; (void)world;
  }

  /* The ONE non-default accessor, and what makes a target a real participant rather than a marker. */
  const FBDamageLayout &DamageLayout() const override { return Spec_.Layout; }

  Systems::FBAutopilot &Autopilot() override { return AP_; }
  Systems::FBFlightControl &FlightControl() override { return FC_; }
  Pilot::FBPilot &PilotSystem() override { return Pilot_; }
  Systems::FBAirframeControls &Controls() override { return Ctrl_; }
  Systems::FBDisplaySystem &Displays() override { return Disp_; }
  Systems::FBAirDataSystem &AirDataSystem() override { return AirData_; }
  Systems::FBNavSystem &NavSystem() override { return Nav_; }
  Systems::FBWarningSystem &WarningSystem() override { return Warn_; }
  Systems::FBRadarAltimeter &RadarAltimeter() override { return RadarAlt_; }
  FBCommandBus &Commands() override { return Cmds_; }
  Sensors::FBDatalinkSystem &Datalink() override { return Datalink_; }
  Sensors::FBRadarSystem &Radar() override { return Radar_; }
  Sensors::FBRwrSystem &Rwr() override { return Rwr_; }
  Sensors::FBIrstSystem &Irst() override { return Irst_; }
  Sensors::FBCountermeasureSystem &Countermeasures() override { return Cm_; }
  Weapons::FBStoresSystem &Stores() override { return Stores_; }
  Weapons::FBGunSystem &Guns() override { return Gun_; }
  const FBState &Telemetry() const override { return State_; }
  const Systems::FBGuidance &LastGuidance() const override { return LastG_; }
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

  Systems::FBAutopilot AP_;
  Systems::FBFlightControl FC_;
  Pilot::FBPilot Pilot_;
  Systems::FBAirframeControls Ctrl_;
  Systems::FBDisplaySystem Disp_;
  Systems::FBAirDataSystem AirData_;
  Systems::FBNavSystem Nav_;
  Systems::FBWarningSystem Warn_;
  Systems::FBRadarAltimeter RadarAlt_;
  FBCommandBus Cmds_;
  Sensors::FBDatalinkSystem Datalink_;
  Sensors::FBRadarSystem Radar_;
  Sensors::FBRwrSystem Rwr_;
  Sensors::FBIrstSystem Irst_;
  Sensors::FBCountermeasureSystem Cm_;
  Weapons::FBStoresSystem Stores_;
  Weapons::FBGunSystem Gun_;
  FBFlightPlan Plan_;
  FBState State_{};
  Systems::FBGuidance LastG_{};
};

} // namespace FlightBox::Modules
#endif
