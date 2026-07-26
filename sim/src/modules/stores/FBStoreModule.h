/* FlightBox — FBStoreModule: the module a RELEASED store flies with. A weapon in flight is structurally
 * the same thing as a jet — one FBModule over one JSBSim model, owned by one units/FBSimUnit, stepped by
 * the same runner, judged by the same monitors — and this class is what makes that literal rather than
 * a claim: it is a full FBModule whose systems are all the airframe-agnostic defaults, because a Mk-82
 * has no autopilot, no pilot, no radar and no displays, and its Run() therefore does the only thing a
 * bomb does: integrate.
 *
 * IT HAS NO PILOT AND NO GUIDANCE, deliberately. Nothing here touches the FDM's control inputs, so the
 * trajectory is the vendored aero model plus gravity and nothing else — which is the entire point of
 * modelling a weapon as its own FDM instance instead of as a hand-written ballistic formula. A guided
 * weapon is a DIFFERENT module (its seeker/autopilot filled in), not a flag on this one.
 *
 * ONE CLASS, N CATALOGUE ENTRIES: the store it flies is the FBStoreSpec handed to the constructor, so
 * every entry of core/FBStore.h registers the same class under its own name (FBStoreModuleRegistration).
 * The FdmModelName comes from that spec — the store's own pinned JSBSim model. */
#ifndef FBSTOREMODULE_H
#define FBSTOREMODULE_H

#include "FBModule.h"
#include "FBStore.h"
#include "FBSystemSlots.h"

namespace FlightBox {

class FBStoreModule : public FBModule {
public:
  explicit FBStoreModule(const FBStoreSpec &spec) : Spec_(spec) {}

  const FBStoreSpec &Spec() const { return Spec_; }

  void AttachFdm(FBFdm &fdm) override { Fdm_ = &fdm; }
  const char *FdmModelName() const override { return Spec_.FdmModel; }
  bool FdmModelVendored() const override { return Spec_.Vendored; }

  /* The whole behaviour of a released store: fixed 100 Hz substeps of its own FDM, no command written
   * to any control channel. Same substep accumulator and spiral guard as every other module, so a
   * store integrates on the same clock as the jet that dropped it. */
  void Run(fb_fdm_state &st, double dt, const FBUnitRegistry *units = nullptr,
           const FBWorld *world = nullptr) override;

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
  FBStoresSystem &Stores() override { return Stores_; }   /* a bomb carries no stores of its own */
  const FBState &Telemetry() const override { return State_; }
  const FBGuidance &LastGuidance() const override { return LastG_; }
  int LastSubsteps() const override { return LastSub_; }

  FBFlightPlan &FlightPlan() override { return Plan_; }
  void SetRunway(const FBRunway &rwy) override { (void)rwy; }
  void SetGroundAsl(float m) override { GroundAslM_ = m; }
  /* A released store takes no mission setup: it was configured by being loaded onto a pylon. Every key
   * is therefore unknown, which is a mission FAIL — and correctly so, since a `set` line aimed at a
   * store could only be a mission that thinks it is declaring an aircraft. */
  bool ApplySetup(const std::string &key, const std::string &value) override {
    (void)key; (void)value;
    return false;
  }

private:
  const FBStoreSpec &Spec_;
  FBFdm *Fdm_ = nullptr;          /* borrowed, never owned (AttachFdm) */
  float GroundAslM_ = 0.0f;
  double AccS_ = 0.0;
  int LastSub_ = 0;

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
  FBStoresSystem Stores_;
  FBFlightPlan Plan_;
  FBState State_{};
  FBGuidance LastG_{};
};

} // namespace FlightBox
#endif
