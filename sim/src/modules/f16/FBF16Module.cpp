#include "FBF16Module.h"
#include "FBF16Hud.h"
#include "FBLog.h"
#include "FBUnits.h"
#include <cerrno>
#include <cmath>
#include <cstdlib>

namespace FlightBox {

FBF16Module::FBF16Module()
    : AP(std::make_unique<FBAutopilot>()),
      FC(std::make_unique<FBFlightControl>(FBFlightControl::F16())),
      Input(std::make_unique<FBInputSystem>()),
      Propulsion(std::make_unique<FBPropulsionSystem>()),
      Disp(std::make_unique<FBF16Hud>()),   /* the F-16's own HUD symbology, not the generic default */
      Chip(std::make_unique<FBF16Max7456>()),
      Sensors(std::make_unique<FBSensorSystem>()),
      Weapons(std::make_unique<FBWeaponSystem>()),
      Defensive(std::make_unique<FBDefensiveSystem>()),
      Comms(std::make_unique<FBCommsSystem>()),
      AirData(std::make_unique<FBAirDataSystem>()),
      RadarAlt(std::make_unique<FBRadarAltimeter>()),
      NavSys(std::make_unique<FBNavSystem>()),
      FireCtrl(std::make_unique<FBF16FireControl>()),
      UfcSys(std::make_unique<FBF16Ufc>()),
      SmsSys(std::make_unique<FBF16Sms>()),
      PilotSys(std::make_unique<FBF16Pilot>()),
      AirframeCtrl(std::make_unique<FBAirframeControls>()) {}   /* NoOp until an airframe is attached */

void FBF16Module::AttachFdm(FBFdm &fdm) {
  Fdm_ = &fdm;
  AirframeCtrl = std::make_unique<FBJsbsimAirframeControls>(fdm);
}

bool FBF16Module::Due(double &accS, double dt, double hz) {
  accS += dt;
  double period = 1.0 / hz;
  if (accS < period) return false;
  accS -= period;
  return true;
}

/* Cycles every system slot the doc/f16/ inventory names (see the header's rate table), then the
 * fixed 100 Hz FDM substeps (spiral guard, <=12/frame): guidance -> FLCS-command -> JSBSim in
 * lockstep. AP->Run() / FC->Run() are the only virtual dispatch INSIDE that inner loop (one call
 * each per substep); every other slot below is throttled OUTSIDE it, at most once per Run(). */
void FBF16Module::Run(fb_fdm_state &st, double dt, const FBWorld *world) {
  if (!Fdm_) return;               /* no airframe attached (FBModule::AttachFdm) — nothing to fly */
  FBFdm &fdm = *Fdm_;
  Input->Run(Mode, dt);            /* HOTAS/ICP: once per Run() call, the coarsest sim tick */
  Propulsion->Run(st, dt);         /* engine-system logic above the raw FDM: same cadence */

  if (Due(SensorAccS, dt, 10.0)) {
    Sensors->Run(SharedState, world, dt);
    /* The HUD's telemetry chain, one throttle group so FireControl always reads Nav's SAME-tick output
     * (see the header's rate table) — `st` is the FDM state as of the END of the PREVIOUS Run() call,
     * same one-tick lag every other Sensor-cadence write already has. */
    AirData->Run(SharedState, st, dt);
    RadarAlt->Run(SharedState, (float)st.elev, GroundAslM);
    NavSys->Run(SharedState, st, dt);
    FireCtrl->Run(SharedState, dt);
    UfcSys->Run(SharedState, dt);
    SmsSys->Run(SharedState, dt);
  }
  if (Due(DisplayAccS, dt, 20.0)) Disp->Run(SharedState, Mode, dt);
  if (Due(WeaponAccS, dt, 20.0)) Weapons->Run(Mode, world, dt);
  if (Due(DefensiveAccS, dt, 5.0)) Defensive->Run(world, dt);
  if (Due(CommsAccS, dt, 1.0)) Comms->Run(dt);
  /* Pilot: the mission brain above Guidance/FlightControl (rate table). Idle (nobody called SetPhase)
   * returns a neutral FBPilotCommands, so ApplyPilotCommands is a no-op until the App starts the phase
   * machine — once it does, this is the takeoff/climb/route chain actually flying the jet. Waypoint
   * capture (Akteurs-Verhalten, FBNavSystem's own job — doc/mission-format.md) runs right after, same
   * cadence: THIS tick's Pilot::Run() flew toward the pre-capture active waypoint. This module never
   * sees whether the mission itself concluded from that same capture — that verdict is a separate,
   * independent judgement the caller owns (core/, not this file). */
  if (Due(PilotAccS, dt, 10.0)) {
    ApplyPilotCommands(PilotSys->Run(SharedState, *AirframeCtrl, st, Plan_, HaveRunway_ ? &Rwy_ : nullptr,
                                     world, dt));
    NavSys->AdvanceWaypoint(Plan_, st.lat, st.lon);
  }

  AccS += dt;
  LastSub = 0;
  for (int k = 0; AccS >= FBFdm::kStepS && k < 12; k++) {
    LastG = AP->Run(st);
    FBControls c = FC->Run(LastG, st);
    fdm.SetControls(c.Roll, c.Pitch, c.Yaw, c.Thr);
    fdm.Step(st);
    AccS -= FBFdm::kStepS;
    LastSub++;
  }
}

/* Applies only the fields FBPilotCommands actually set (Guidance != None, each std::optional present) —
 * an Idle-phase neutral FBPilotCommands (every field default/unset) reaches here and calls NOTHING,
 * which is what keeps an un-started pilot (Phase::Idle) bit-identical to not having one (see Run()'s
 * banner). */
void FBF16Module::ApplyPilotCommands(const FBPilotCommands &c) {
  switch (c.Guidance) {
    case FBPilotGuidance::Manual:
      AP->SetManual(c.ManualRoll, c.ManualPitch, c.ManualYaw, c.ManualThr);
      break;
    case FBPilotGuidance::Direct:
      AP->SetDirect(c.TargetLatDeg, c.TargetLonDeg, c.TargetAltM, c.TargetSpeedKt * kKtToMs);
      break;
    case FBPilotGuidance::Course:
      AP->SetCourse(c.TargetLatDeg, c.TargetLonDeg, c.CourseDeg, c.TargetAltM, c.GlidepathDeg,
                    c.TargetSpeedKt * kKtToMs);
      break;
    case FBPilotGuidance::None:
      break;   /* leave whatever guidance is already running untouched */
  }
  if (c.GearDown) AirframeCtrl->SetGear(*c.GearDown);
  if (c.Speedbrake) AirframeCtrl->SetSpeedbrake(*c.Speedbrake);
  if (c.WheelBrakeLeft || c.WheelBrakeRight)
    AirframeCtrl->SetWheelBrakes(c.WheelBrakeLeft.value_or(0.0), c.WheelBrakeRight.value_or(0.0));
  if (c.NosewheelSteer) AirframeCtrl->SetNosewheelSteer(*c.NosewheelSteer);
  if (c.EngineStart) *c.EngineStart ? AirframeCtrl->EngineStart() : AirframeCtrl->EngineCutoff();
}

/* Boundary input (mission-file text, defensive checks per CLAUDE.md's C++ conventions). STRICT: the whole
 * value must be one finite number and nothing else. A silent 0.0 default here is worse than a hard
 * failure — `set fuel_lbs FULL` or a thousands separator would spawn the jet with an empty tank, report
 * success, and JSBSim would kill the engine minutes later in the air with nothing in events.log naming
 * the cause. The parser is FBMissionFile's Trim'ed value, so no leading/trailing space is expected. */
namespace {
bool ParseDouble(const std::string &s, double &out) {
  if (s.empty()) return false;
  char *end = nullptr;
  errno = 0;
  double v = std::strtod(s.c_str(), &end);
  if (end != s.c_str() + s.size()) return false;   /* trailing garbage: "3,000", "120kt", "FULL" */
  if (errno == ERANGE || !std::isfinite(v)) return false;
  out = v;
  return true;
}

/* One rejection, one greppable event naming BOTH the key and the raw text — the caller turns the false
 * into a mission FAIL (FBMissionBoot.h), so this is the only place the actual value is still known. */
bool RejectSetup(const char *reason, const std::string &key, const std::string &value) {
  FBLog::Error("module", "SET_INVALID_VALUE", {{"key", key}, {"value", value}, {"reason", reason}});
  return false;
}
} // namespace

bool FBF16Module::ApplySetup(const std::string &key, const std::string &value) {
  if (!Fdm_) return false;   /* setup lines describe the airframe's state — none to apply without one */
  if (key == "gear") {
    if (value != "up" && value != "down") return RejectSetup("want up|down", key, value);
    AirframeCtrl->SetGear(value == "down");
    return true;
  }
  if (key == "fuel_lbs") {
    double lbs = 0.0;
    if (!ParseDouble(value, lbs)) return RejectSetup("not a number", key, value);
    if (lbs < 0.0) return RejectSetup("negative fuel load", key, value);
    Fdm_->SetFuelTotalLbs(lbs);
    return true;
  }
  if (key == "fuel_pct") {
    double pct = 0.0;
    if (!ParseDouble(value, pct)) return RejectSetup("not a number", key, value);
    if (pct < 0.0 || pct > 100.0) return RejectSetup("outside 0..100", key, value);
    Fdm_->SetFuelPct(pct);
    return true;
  }
  return false;   /* unknown key: FBMissionBoot.h logs SET_UNKNOWN_KEY with the key AND value */
}

} // namespace FlightBox
