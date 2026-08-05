/* FlightBox — FBGroundModule: a STATIC GROUND TARGET. Structurally FBStoreModule MINUS one thing rather
 * than plus one — a released bomb has no pilot and no guidance but still integrates; this has not even
 * that. Why it has NO AIRFRAME at all: doc/weapons-and-damage.md §10.3. */
#ifndef FBGROUNDMODULE_H
#define FBGROUNDMODULE_H

#include "FBGroundTarget.h"
#include "FBModule.h"
#include "FBSystemSlots.h"

namespace FlightBox::Modules {

class FBGroundModule : public FBModule {
public:
  /* Every receiver AND the transmitter down: the radar slot's default is powered, so a bunker used to
   * put an AirborneFireControl beam on the air, and its transponder answered Mode 4 as friendly from
   * outside the power gate. doc/air-to-ground.md §6. */
  explicit FBGroundModule(const FBGroundTargetSpec &spec) : Spec_(spec) {
    Radar_.SetPowered(false);
    Radar_.SetIffTransponder(false);
    Rwr_.SetPowered(false);
    Visual_.SetPowered(false);
    /* Twenty declarations for a bunker, and the reason is the telemetry column layout rather than the
     * airframe — the same one FBStoreModule states. */
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

  const FBGroundTargetSpec &Spec() const { return Spec_; }

  Units::FBUnitKind UnitKind() const override { return Units::FBUnitKind::Ground; }

  /* Empty on purpose: the whole per-tick behaviour of a target is that its pose is the declared one. */
  void Run(Fdm::fb_fdm_state &st, double dt, const Units::FBUnitRegistry *units = nullptr,
           const World::FBWorld *world = nullptr) override {
    (void)st; (void)dt; (void)units; (void)world;
  }

  /* The ONE non-default accessor, and what makes a target a real participant rather than a marker. */
  const FBDamageLayout &DamageLayout() const override { return Spec_.Layout; }

  const FBState &Telemetry() const override { return State_; }
  const Systems::FBGuidance &LastGuidance() const override { return LastG_; }

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
