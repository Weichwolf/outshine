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

#include "FBMissileArSeeker.h"
#include "FBMissileGuidance.h"
#include "FBMissileIrSeeker.h"
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

  /* The passive head is the one slot on this module that walks the registry by identity, so it is the
   * one that has to know which unit it is. */
  void SetUnitIdentity(int unitId, FBUnitTeam team) override { Ar_.SetIdentity(unitId, team); }

  void AttachFdm(Fdm::FBFdm &fdm) override { Fdm_ = &fdm; }
  const char *FdmModelName() const override { return Spec_.FdmModel; }

  /* Applied once before the first tick. The release time SEEDS this module's clock, so the round's
   * "now" is the one every other unit and every message age is stamped against. */
  void ProgramRelease(const FBStoreRelease &rel) override;

  void Run(Fdm::fb_fdm_state &st, double dt, const Units::FBUnitRegistry *units = nullptr,
           const World::FBWorld *world = nullptr) override;

  const FBState &Telemetry() const override { return State_; }
  const Systems::FBGuidance &LastGuidance() const override { return LastG_; }
  int LastSubsteps() const override { return LastSub_; }
  void SetGroundAsl(float m) override { GroundAslM_ = m; }

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
  FBMissileSeeker Seeker_;      /* active/semi-active rounds; dark on an infrared one */
  FBMissileIrSeeker Ir_;                  /* infrared rounds; caged on the other two */
  Sensors::FBVisualSystem Visual_;
  FBMissileArSeeker Ar_;                  /* anti-radiation rounds; unpowered on every other one */
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
