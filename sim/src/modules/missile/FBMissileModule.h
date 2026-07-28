/* FlightBox — FBMissileModule: what a GUIDED weapon flies with, the counterpart of FBStoreModule. A
 * bomb's module only integrates; a missile fills three REAL slots (seeker, uplink receiver, guidance)
 * and cycles them — that is the whole difference. ONE CLASS, N CATALOGUE ENTRIES: the round it flies is
 * the FBStoreSpec handed to the constructor.
 *
 * RATES: the seeker and the guidance run INSIDE the 100 Hz substep loop, because a round closing at
 * 1.5 km/s covers 15 m per 10 ms and a pilot's 10 Hz decision tick would be a 150 m guidance quantum.
 * The uplink runs once per Run() — the launcher cannot produce a fresher estimate than its radar frame.
 * doc/weapons-and-damage.md §10.2. */
#ifndef FBMISSILEMODULE_H
#define FBMISSILEMODULE_H

#include "FBMissileGuidance.h"
#include "FBMissileSeeker.h"
#include "FBMissileUplink.h"
#include "FBModule.h"
#include "FBStore.h"
#include "FBSystemSlots.h"

namespace FlightBox::Modules {

class FBMissileModule : public FBModule {
public:
  explicit FBMissileModule(const FBStoreSpec &spec);

  const FBStoreSpec &Spec() const { return Spec_; }

  void AttachFdm(Fdm::FBFdm &fdm) override { Fdm_ = &fdm; }
  const char *FdmModelName() const override { return Spec_.FdmModel; }

  /* Applied once before the first tick. The release time SEEDS this module's clock, so the round's
   * "now" is the one every other unit and every message age is stamped against. */
  void ProgramRelease(const FBStoreRelease &rel) override;

  void Run(Fdm::fb_fdm_state &st, double dt, const Units::FBUnitRegistry *units = nullptr,
           const World::FBWorld *world = nullptr) override;

  Systems::FBAutopilot &Autopilot() override { return AP_; }
  Systems::FBFlightControl &FlightControl() override { return FC_; }
  FBMissileGuidance &PilotSystem() override { return Guidance_; }   /* covariant: FBPilot& on the base */
  Systems::FBAirframeControls &Controls() override { return Ctrl_; }
  Systems::FBDisplaySystem &Displays() override { return Disp_; }
  Systems::FBAirDataSystem &AirDataSystem() override { return AirData_; }
  Systems::FBNavSystem &NavSystem() override { return Nav_; }
  Systems::FBWarningSystem &WarningSystem() override { return Warn_; }
  Systems::FBRadarAltimeter &RadarAltimeter() override { return RadarAlt_; }
  FBCommandBus &Commands() override { return Cmds_; }
  FBMissileUplink &Datalink() override { return Uplink_; }          /* covariant */
  FBMissileSeeker &Radar() override { return Seeker_; }             /* covariant */
  /* The slots exist because every module carries the same categories; these are the defaults, never
   * cycled and powered down at construction so nothing they hold can be mistaken for a picture. */
  Sensors::FBRwrSystem &Rwr() override { return Rwr_; }
  Sensors::FBIrstSystem &Irst() override { return Irst_; }
  Sensors::FBCountermeasureSystem &Countermeasures() override { return Cm_; }
  Weapons::FBStoresSystem &Stores() override { return Stores_; }             /* a round carries no stores */
  Weapons::FBGunSystem &Guns() override { return Gun_; }                     /* a round carries no gun */
  const FBState &Telemetry() const override { return State_; }
  const Systems::FBGuidance &LastGuidance() const override { return LastG_; }
  int LastSubsteps() const override { return LastSub_; }

  FBFlightPlan &FlightPlan() override { return Plan_; }
  void SetRunway(const FBRunway &rwy) override { (void)rwy; }
  void SetGroundAsl(float m) override { GroundAslM_ = m; }
  /* A launched round takes no mission setup: it was configured by being loaded and then programmed at
   * launch, so any `set` key is unknown and the caller turns that into a mission FAIL. */
  bool ApplySetup(const std::string &key, const std::string &value) override {
    (void)key; (void)value;
    return false;
  }

private:
  const FBStoreSpec &Spec_;
  Fdm::FBFdm *Fdm_ = nullptr;          /* borrowed, never owned */
  float GroundAslM_ = 0.0f;
  double AccS_ = 0.0;
  double SimTimeS_ = 0.0;         /* seeded with the release time */
  int LastSub_ = 0;

  Systems::FBAutopilot AP_;
  Systems::FBFlightControl FC_;
  FBMissileGuidance Guidance_;
  FBMissileSeeker Seeker_;
  Sensors::FBRwrSystem Rwr_;
  Sensors::FBIrstSystem Irst_;
  Sensors::FBCountermeasureSystem Cm_;
  FBMissileUplink Uplink_;
  Systems::FBAirframeControls Ctrl_;
  Systems::FBDisplaySystem Disp_;
  Systems::FBAirDataSystem AirData_;
  Systems::FBNavSystem Nav_;
  Systems::FBWarningSystem Warn_;
  Systems::FBRadarAltimeter RadarAlt_;
  FBCommandBus Cmds_;
  Weapons::FBStoresSystem Stores_;
  Weapons::FBGunSystem Gun_;
  FBFlightPlan Plan_;
  FBState State_{};
  Systems::FBGuidance LastG_{};
};

} // namespace FlightBox::Modules
#endif
