/* FlightBox — FBStoreModule: the module a RELEASED store flies with, and what makes "a weapon in flight
 * is structurally a jet" literal rather than a claim — a full FBModule whose slots are all defaults,
 * whose Run() only integrates. NO PILOT AND NO GUIDANCE, deliberately: nothing here touches a control
 * channel, so the trajectory is the model's own aero deck plus gravity, which is the entire point of
 * giving a weapon its own FDM instead of a hand-written ballistic formula. A guided weapon is a
 * DIFFERENT module, not a flag on this one. ONE CLASS, N CATALOGUE ENTRIES.
 * doc/weapons-and-damage.md §10.1. */
#ifndef FBSTOREMODULE_H
#define FBSTOREMODULE_H

#include "FBModule.h"
#include "FBStore.h"
#include "FBSystemSlots.h"

namespace FlightBox::Modules {

class FBStoreModule : public FBModule {
public:
  explicit FBStoreModule(const FBStoreSpec &spec) : Spec_(spec) { Rwr_.SetPowered(false); }

  const FBStoreSpec &Spec() const { return Spec_; }

  void AttachFdm(Fdm::FBFdm &fdm) override { Fdm_ = &fdm; }
  const char *FdmModelName() const override { return Spec_.FdmModel; }

  /* Fixed 100 Hz substeps of its own FDM, no command to any control channel — the same accumulator and
   * spiral guard as every other module, so a store integrates on the clock of the jet that dropped it. */
  void Run(Fdm::fb_fdm_state &st, double dt, const Units::FBUnitRegistry *units = nullptr,
           const World::FBWorld *world = nullptr) override;

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
  /* The slots exist because every module carries the same categories; these are the defaults, never
   * cycled and powered down at construction so nothing they hold can be mistaken for a picture. */
  Sensors::FBRwrSystem &Rwr() override { return Rwr_; }
  Sensors::FBIrstSystem &Irst() override { return Irst_; }
  Sensors::FBCountermeasureSystem &Countermeasures() override { return Cm_; }
  Weapons::FBStoresSystem &Stores() override { return Stores_; }
  Weapons::FBGunSystem &Guns() override { return Gun_; }
  const FBState &Telemetry() const override { return State_; }
  const Systems::FBGuidance &LastGuidance() const override { return LastG_; }
  int LastSubsteps() const override { return LastSub_; }

  FBFlightPlan &FlightPlan() override { return Plan_; }
  void SetRunway(const FBRunway &rwy) override { (void)rwy; }
  void SetGroundAsl(float m) override { GroundAslM_ = m; }
  /* A released store was configured by being loaded onto a pylon, so any `set` key is unknown and voids
   * the spawn — correctly, since such a line could only be a mission that thinks it declares a jet. */
  bool ApplySetup(const std::string &key, const std::string &value) override {
    (void)key; (void)value;
    return false;
  }

private:
  const FBStoreSpec &Spec_;
  Fdm::FBFdm *Fdm_ = nullptr;          /* borrowed, never owned */
  float GroundAslM_ = 0.0f;
  double AccS_ = 0.0;
  int LastSub_ = 0;

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
