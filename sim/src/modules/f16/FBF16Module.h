/* FlightBox — FBF16Module: the F-16, today's one registered FBModule. Composes the DEFAULT systems
 * (systems/FBAutopilot, systems/FBFlightControl — unmodified) with the F-16 gain preset
 * (FBFlightControl::F16()), and cycles the full system-slot set the doc/f16/ inventory names — Guidance/FCS,
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
 *   Comms/Datalink (FBF16Datalink)           5 Hz        accumulator-throttled inside Run(); the NET
 *                                                        itself cycles at 1 Hz inside the system
 *                                                        (FBDatalinkSystem::kNetPeriodS) — the slot is
 *                                                        entered faster than that only so a held track's
 *                                                        AGE is observable between net cycles, never to
 *                                                        refresh the picture sooner
 *   Pilot (FBPilot, the mission-level brain ABOVE  10 Hz  accumulator-throttled inside Run(); its
 *   Guidance/FlightControl — core/ architecture           FBPilotCommands are applied to Autopilot()/
 *   banner)                                                Controls() only where a field is actually
 *                                                          set (std::optional/Guidance::None = "leave
 *                                                          it") — Idle (the phase-machine's boot state)
 *                                                          stays neutral, so composing the pilot changes
 *                                                          NOTHING until the App actually starts it
 *                                                          (SetPhase(Preflight), see FBAppNative.cpp
 *                                                          RunMission / FBAppWasm.cpp's mission boot).
 * All NoOp defaults cost one throttle comparison when not due, and nothing when they are (empty
 * virtual call) — no per-frame heap allocation, no dispatch inside the 100 Hz substep's inner math. */
#ifndef FBF16MODULE_H
#define FBF16MODULE_H

#include <memory>
#include "FBAirDataSystem.h"
#include "FBAirframeControls.h"
#include "FBAutopilot.h"
#include "FBF16Datalink.h"
#include "FBF16FireControl.h"
#include "FBF16Max7456.h"
#include "FBF16Pilot.h"
#include "FBF16Sms.h"
#include "FBF16Ufc.h"
#include "FBFlightControl.h"
#include "FBFlightPlan.h"
#include "FBModule.h"
#include "FBNavSystem.h"
#include "FBPilot.h"
#include "FBRadarAltimeter.h"
#include "FBRunway.h"
#include "FBSystemSlots.h"
#include "FBMasterMode.h"

namespace FlightBox {

class FBF16Module : public FBModule {
public:
  FBF16Module();

  /* Borrows the airframe and swaps the NoOp airframe-controls default for the real, FDM-bound one —
   * see FBModule::AttachFdm. Until it is called the module has no airframe and Run() has nothing to
   * fly. */
  void AttachFdm(FBFdm &fdm) override;

  /* vendor/jsbsim/aircraft/f16 — the vanilla full-scale model (CLAUDE.md Prinzip 5). Deliberately NOT
   * derived from the registry name: the two happen to coincide today, they are not the same thing. */
  const char *FdmModelName() const override { return "f16"; }

  void Run(fb_fdm_state &st, double dt, const FBUnitRegistry *units = nullptr,
           const FBWorld *world = nullptr) override;

  /* The HUD's telemetry chain writes HERE (SharedState), not into the App's own FBState — the App must
   * seed its per-frame FBState from this BEFORE overwriting the fields it computes itself (pose/sun/
   * moon/...), e.g. `FBState hs = F16->Telemetry(); hs.roll = ...;`, so BuildHud sees both. */
  const FBState &Telemetry() const override { return SharedState; }

  FBAutopilot &Autopilot() override { return *AP; }
  FBFlightControl &FlightControl() override { return *FC; }
  FBDisplaySystem &Displays() override { return *Disp; }   /* wire into FBRenderer::SetHudDisplay */
  FBF16Max7456 &Max7456() { return *Chip; }       /* the MAX7456-chip-specific hook, see its banner */
  FBNavSystem &NavSystem() override { return *NavSys; }   /* steerpoint/bullseye (SetSteerpoint/SetBullseye) */
  FBDatalinkSystem &Datalink() override { return *Datalink_; }   /* the F-16's TNDL terminal */
  FBF16Ufc &Ufc() { return *UfcSys; }             /* ALOW/selected-steerpoint placeholder setup */
  FBF16Sms &Sms() { return *SmsSys; }             /* master-arm placeholder setup */
  const FBGuidance &LastGuidance() const override { return LastG; }
  int LastSubsteps() const override { return LastSub; }

  /* Ground ASL (m) under the aircraft, the SAME DEM sample the App already resolved for
   * FBRenderer::SetAgl (fb_stream_ground) — call before Run() each frame; FBRadarAltimeter reuses it
   * rather than re-querying terrain. */
  void SetGroundAsl(float m) override { GroundAslM = m; }

  /* Identity for the systems that observe other units (FBModule::SetUnitIdentity) — today the
   * terminal, tomorrow the radar. */
  void SetUnitIdentity(int unitId, FBUnitTeam team) override { Datalink_->SetIdentity(unitId, team); }

  FBMasterMode GetMasterMode() const { return Mode; }
  void SetMasterMode(FBMasterMode m) { Mode = m; }

  /* The pilot (see the rate table) + what it flies/lands on: FlightPlan/Runway are simple accessors the
   * App fills at boot (mission setup, not a per-frame write); PilotSys is the F-16 skeleton (currently
   * the unmodified FBPilot default) exposed for boot/test phase-machine access (mission boot). */
  FBF16Pilot &PilotSystem() override { return *PilotSys; }   /* covariant: FBModule::PilotSystem() returns FBPilot& */
  FBAirframeControls &Controls() override { return *AirframeCtrl; }
  FBAirDataSystem &AirDataSystem() override { return *AirData; }   /* the ADC telemetry source (mission-runner Bus) */
  FBFlightPlan &FlightPlan() override { return Plan_; }
  void SetRunway(const FBRunway &rwy) override { Rwy_ = rwy; HaveRunway_ = true; }

  /* `gear` (up/down — an air start spawns retracted), `fuel_lbs`, `fuel_pct` (0..100), and the
   * terminal's four switches — `datalink` (on/off: power), `datalink_xmt` (on/off: XMT/EMCON),
   * `datalink_filter` (fr/fl/off: the HSD contact filter), `datalink_range_nm` — see
   * doc/mission-format.md. An unknown key OR an unparsable/out-of-range value returns false (Runner-level
   * mission FAIL): a mission that declares a state this airframe cannot take must not start silently in
   * some other state. */
  bool ApplySetup(const std::string &key, const std::string &value) override;

private:
  void ApplyPilotCommands(const FBPilotCommands &c);
  static bool Due(double &accS, double dt, double hz);   /* throttle helper for the slower slots */

  /* Owned through base pointers (not value members) so a future module can substitute an override
   * without slicing; the F-16 composes the unmodified DEFAULTS. */
  std::unique_ptr<FBAutopilot> AP;
  std::unique_ptr<FBFlightControl> FC;

  std::unique_ptr<FBInputSystem> Input;
  std::unique_ptr<FBPropulsionSystem> Propulsion;
  std::unique_ptr<FBDisplaySystem> Disp;
  std::unique_ptr<FBF16Max7456> Chip;   /* MAX7456-chip-specific hook, see FBF16Max7456's banner */
  std::unique_ptr<FBSensorSystem> Sensors;
  std::unique_ptr<FBWeaponSystem> Weapons;
  std::unique_ptr<FBDefensiveSystem> Defensive;
  std::unique_ptr<FBF16Datalink> Datalink_;   /* the Comms slot, filled with the F-16's own terminal */

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
  std::unique_ptr<FBAirframeControls> AirframeCtrl;   /* NoOp until AttachFdm, FBJsbsimAirframeControls after */
  FBFdm *Fdm_ = nullptr;                              /* borrowed, never owned (AttachFdm) */
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
  /* The module's own sim clock (accumulated dt): the datalink stamps and ages its messages in absolute
   * sim seconds, so its picture cannot depend on how often this module happens to cycle the slot. */
  double SimTimeS = 0.0;
};

} // namespace FlightBox
#endif
