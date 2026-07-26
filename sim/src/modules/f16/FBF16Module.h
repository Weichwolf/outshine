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
 *   Sensors, AirData, RadarAlt, Nav,          10 Hz       accumulator-throttled inside Run(), ONE group
 *   FireControl, Ufc, Sms                                (the HUD's telemetry chain updates together —
 *                                                          FireControl reads Nav's SAME-tick output)
 *   Defensive                                5 Hz        accumulator-throttled inside Run()
 *   Comms                                    1 Hz        accumulator-throttled inside Run()
 *   Pilot (FBPilot, the mission-level brain ABOVE  10 Hz  accumulator-throttled inside Run(); its
 *   Guidance/FlightControl — core/ architecture           FBPilotCommands are applied to Autopilot()/
 *   banner)                                                Controls() only where a field is actually
 *                                                          set (std::optional/Guidance::None = "leave
 *                                                          it") — Idle (the phase-machine's boot state)
 *                                                          stays neutral, so composing the pilot changes
 *                                                          NOTHING until the App actually starts it
 *                                                          (SetPhase(Preflight), see FBAppNative.cpp
 *                                                          RunMission/--pilot).
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
#include "FBAirDataSystem.h"
#include "FBAirframeControls.h"
#include "FBAutopilot.h"
#include "FBF16FireControl.h"
#include "FBF16Max7456.h"
#include "FBF16Pilot.h"
#include "FBF16Sms.h"
#include "FBF16Ufc.h"
#include "FBFlightControl.h"
#include "FBFlightPlan.h"
#include "FBModule.h"
#include "FBNavSystem.h"
#include "FBPathPlan.h"
#include "FBPilot.h"
#include "FBRadarAltimeter.h"
#include "FBRunway.h"
#include "FBSystemSlots.h"
#include "FBMasterMode.h"

namespace FlightBox {

class FBTerrainField;

class FBF16Module : public FBModule {
public:
  FBF16Module();

  void Run(fb_fdm_state &st, double dt, const FBWorld *world = nullptr) override;

  /* The HUD's telemetry chain writes HERE (SharedState), not into the App's own FBState — the App must
   * seed its per-frame FBState from this BEFORE overwriting the fields it computes itself (pose/sun/
   * moon/...), e.g. `FBState hs = F16->Telemetry(); hs.roll = ...;`, so BuildHud sees both. */
  const FBState &Telemetry() const { return SharedState; }

  FBAutopilot &Autopilot() { return *AP; }
  FBFlightControl &FlightControl() { return *FC; }
  FBDisplaySystem &Displays() { return *Disp; }   /* wire into FBRenderer::SetHudDisplay */
  FBF16Max7456 &Max7456() { return *Chip; }       /* the MAX7456-chip-specific hook, see its banner */
  FBNavSystem &Nav() { return *NavSys; }          /* steerpoint/bullseye setup (SetSteerpoint/SetBullseye) */
  FBF16Ufc &Ufc() { return *UfcSys; }             /* ALOW/selected-steerpoint placeholder setup */
  FBF16Sms &Sms() { return *SmsSys; }             /* master-arm placeholder setup */
  const FBGuidance &LastGuidance() const { return LastG; }
  int LastSubsteps() const { return LastSub; }

  /* Ground ASL (m) under the aircraft, the SAME DEM sample the App already resolved for
   * FBRenderer::SetAgl (fb_stream_ground) — call before Run() each frame; FBRadarAltimeter reuses it
   * rather than re-querying terrain. */
  void SetGroundAsl(float m) { GroundAslM = m; }

  /* LOWLEVEL wander planner: the module owns the instance once the App configures one (opt-in — not
   * every boot mode plans). nullptr until configured. */
  void ConfigurePathPlan(FBTerrainField *field, double centerLat, double centerLon, double fenceRadiusM,
                         unsigned seed) {
    Plan = std::make_unique<FBPathPlan>(field, centerLat, centerLon, fenceRadiusM, seed);
  }
  FBPathPlan *PathPlan() { return Plan.get(); }

  FBMasterMode GetMasterMode() const { return Mode; }
  void SetMasterMode(FBMasterMode m) { Mode = m; }

  /* The pilot (see the rate table) + what it flies/lands on: FlightPlan/Runway are simple accessors the
   * App fills at boot (mission setup, not a per-frame write); PilotSys is the F-16 skeleton (currently
   * the unmodified FBPilot default) exposed for boot/test phase-machine access (e.g. ?ap=pilot). */
  FBF16Pilot &PilotSystem() { return *PilotSys; }
  FBAirframeControls &Controls() { return *AirframeCtrl; }
  FBFlightPlan &FlightPlan() { return Plan_; }
  void SetRunway(const FBRunway &rwy) { Rwy_ = rwy; HaveRunway_ = true; }

private:
  void ApplyPilotCommands(const FBPilotCommands &c);
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

  /* The HUD's telemetry chain (see the rate table): generic systems/ defaults + the two F-16-specific
   * placeholders (FireControl's 'B' slant range, Ufc/Sms). */
  std::unique_ptr<FBAirDataSystem> AirData;
  std::unique_ptr<FBRadarAltimeter> RadarAlt;
  std::unique_ptr<FBNavSystem> NavSys;
  std::unique_ptr<FBF16FireControl> FireCtrl;
  std::unique_ptr<FBF16Ufc> UfcSys;
  std::unique_ptr<FBF16Sms> SmsSys;
  float GroundAslM = 0.0f;

  /* The pilot + what it commands beyond the FDM (see the rate table + FlightPlan()/SetRunway()). */
  std::unique_ptr<FBF16Pilot> PilotSys;
  std::unique_ptr<FBAirframeControls> AirframeCtrl;
  FBFlightPlan Plan_;
  FBRunway Rwy_;
  bool HaveRunway_ = false;
  double PilotAccS = 0.0;

  FBMasterMode Mode = FBMasterMode::Nav;
  FBState SharedState{};   /* Sensors WRITE, Displays READ — no display queries a sensor directly */

  FBGuidance LastG{};
  double AccS = 0.0;
  int LastSub = 0;
  double DisplayAccS = 0.0, SensorAccS = 0.0, WeaponAccS = 0.0, DefensiveAccS = 0.0, CommsAccS = 0.0;
};

} // namespace FlightBox
#endif
