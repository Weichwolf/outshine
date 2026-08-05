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
  /* Every receiver AND the transmitter down: the radar slot's default is powered, so a falling bomb
   * used to put an AirborneFireControl beam on the air, and its transponder answered Mode 4 as
   * friendly from outside the power gate. doc/air-to-ground.md §6. */
  explicit FBStoreModule(const FBStoreSpec &spec) : Spec_(spec) {
    Radar_.SetPowered(false);
    Radar_.SetIffTransponder(false);
    Rwr_.SetPowered(false);
    Visual_.SetPowered(false);
    /* A BOMB STILL DECLARES A GUN, and that is not an oversight: units/FBSimUnit registers fifteen of
     * these slots on the telemetry bus by position, so dropping one here would shift every column of
     * every telemetry.csv. Pruning is a separate, measured round — doc/architecture.md §Gaps. */
    DeclareAutopilot(AP_);
    DeclareFlightControl(FC_);
    DeclarePilotSystem(Pilot_);
    DeclareControls(Ctrl_);
    DeclareDisplays(Disp_);
    DeclareAirDataSystem(AirData_);
    DeclareNavSystem(Nav_);
    DeclareWarningSystem(Warn_);
    DeclareRadarAltimeter(RadarAlt_);
    DeclareCommands(Cmds_);
    DeclareDatalink(Datalink_);
    DeclareRadar(Radar_);
    DeclareRwr(Rwr_);
    DeclareIrst(Irst_);
    DeclareVisual(Visual_);
    DeclareCountermeasures(Cm_);
    DeclareStores(Stores_);
    DeclareGuns(Gun_);
    DeclareFlightPlan(Plan_);
  }

  const FBStoreSpec &Spec() const { return Spec_; }

  void AttachFdm(Fdm::FBFdm &fdm) override { Fdm_ = &fdm; }
  const char *FdmModelName() const override { return Spec_.FdmModel; }

  /* Fixed 100 Hz substeps of its own FDM, no command to any control channel — the same accumulator and
   * spiral guard as every other module, so a store integrates on the clock of the jet that dropped it. */
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
  Sensors::FBVisualSystem Visual_;
  Sensors::FBCountermeasureSystem Cm_;
  Weapons::FBStoresSystem Stores_;
  Weapons::FBGunSystem Gun_;
  FBFlightPlan Plan_;
  FBState State_{};
  Systems::FBGuidance LastG_{};
};

} // namespace FlightBox::Modules
#endif
