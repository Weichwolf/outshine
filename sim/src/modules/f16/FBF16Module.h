/* FlightBox — FBF16Module: the F-16, today's one registered FBModule. Composes the DEFAULT systems
 * (systems/FBAutopilot, systems/FBFlightControl — unmodified) with the F-16 gain preset
 * (FBFlightControl::F16()), owns the LOWLEVEL wander planner (systems/FBPathPlan) once the App
 * configures one, and cycles the full system-slot set the doc/f16/ inventory names — Guidance/FCS,
 * Input/HOTAS, Propulsion, Displays, Sensors, Weapons, Defensive, Comms — each at its own rate. This
 * is the ONE place F-16 behavior lives; it is also the place a future F-16-specific override (a real
 * radar model, a real HOTAS binding, ...) gets hung, by replacing one slot's default with a subclass.
 *
 * Rate table (System -> rate -> mechanism):
 *   Guidance (FBAutopilot) / FlightControl   100 Hz     fixed substep loop (spiral-guarded, <=12/frame)
 *   Input/HOTAS, Propulsion                  ~frame     once per Run() call (the coarsest sim tick)
 *   Displays                                 20 Hz       accumulator-throttled inside Run()
 *   Weapons                                  20 Hz       accumulator-throttled inside Run()
 *   Sensors                                  10 Hz       accumulator-throttled inside Run()
 *   Defensive                                5 Hz        accumulator-throttled inside Run()
 *   Comms                                    1 Hz        accumulator-throttled inside Run()
 *   PathPlan                                 App-owned cadence (see FBF16Module.cpp banner) — the one
 *                                             exception; App-side planner-vs-fan-vs-fixed dispatch
 *                                             differs between the WASM and native oracle today, so it
 *                                             is not unified into this Run() (would change either
 *                                             app's behavior). The module still OWNS the FBPathPlan
 *                                             instance (ConfigurePathPlan/PathPlan()), not the App.
 * All NoOp defaults cost one throttle comparison when not due, and nothing when they are (empty
 * virtual call) — no per-frame heap allocation, no dispatch inside the 100 Hz substep's inner math. */
#ifndef FBF16MODULE_H
#define FBF16MODULE_H

#include <memory>
#include "FBModule.h"
#include "FBAutopilot.h"
#include "FBF16Max7456.h"
#include "FBFlightControl.h"
#include "FBPathPlan.h"
#include "FBSystemSlots.h"
#include "FBMasterMode.h"

namespace FlightBox {

class FBTerrainField;

class FBF16Module : public FBModule {
public:
  FBF16Module();

  void Run(fb_fdm_state &st, double dt, const FBWorld *world = nullptr) override;

  FBAutopilot &Autopilot() { return *AP; }
  FBFlightControl &FlightControl() { return *FC; }
  FBDisplaySystem &Displays() { return *Disp; }   /* wire into FBRenderer::SetHudDisplay */
  FBF16Max7456 &Max7456() { return *Chip; }       /* the MAX7456-chip-specific hook, see its banner */
  const FBGuidance &LastGuidance() const { return LastG; }
  int LastSubsteps() const { return LastSub; }

  /* LOWLEVEL wander planner: the module owns the instance once the App configures one (opt-in — not
   * every boot mode plans). nullptr until configured. */
  void ConfigurePathPlan(FBTerrainField *field, double centerLat, double centerLon, double fenceRadiusM,
                         unsigned seed) {
    Plan = std::make_unique<FBPathPlan>(field, centerLat, centerLon, fenceRadiusM, seed);
  }
  FBPathPlan *PathPlan() { return Plan.get(); }

  FBMasterMode GetMasterMode() const { return Mode; }
  void SetMasterMode(FBMasterMode m) { Mode = m; }

private:
  static bool Due(double &accS, double dt, double hz);   /* throttle helper for the slower slots */

  /* Owned through base pointers (not value members) so a future module can substitute an override
   * without slicing; the F-16 composes the unmodified DEFAULTS. */
  std::unique_ptr<FBAutopilot> AP;
  std::unique_ptr<FBFlightControl> FC;
  std::unique_ptr<FBPathPlan> Plan;   /* nullptr unless ConfigurePathPlan was called */

  std::unique_ptr<FBInputSystem> Input;
  std::unique_ptr<FBPropulsionSystem> Propulsion;
  std::unique_ptr<FBDisplaySystem> Disp;
  std::unique_ptr<FBF16Max7456> Chip;   /* MAX7456-chip-specific hook, see FBF16Max7456's banner */
  std::unique_ptr<FBSensorSystem> Sensors;
  std::unique_ptr<FBWeaponSystem> Weapons;
  std::unique_ptr<FBDefensiveSystem> Defensive;
  std::unique_ptr<FBCommsSystem> Comms;

  FBMasterMode Mode = FBMasterMode::Nav;
  FBState SharedState{};   /* Sensors WRITE, Displays READ — no display queries a sensor directly */

  FBGuidance LastG{};
  double AccS = 0.0;
  int LastSub = 0;
  double DisplayAccS = 0.0, SensorAccS = 0.0, WeaponAccS = 0.0, DefensiveAccS = 0.0, CommsAccS = 0.0;
};

} // namespace FlightBox
#endif
