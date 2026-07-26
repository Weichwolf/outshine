#include "FBF16Module.h"
#include "FBF16Hud.h"
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
      AirframeCtrl(std::make_unique<FBJsbsimAirframeControls>()) {}

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
    ApplyPilotCommands(PilotSys->Run(SharedState, st, Plan_, HaveRunway_ ? &Rwy_ : nullptr, world, dt));
    NavSys->AdvanceWaypoint(Plan_, st.lat, st.lon);
  }

  AccS += dt;
  LastSub = 0;
  for (int k = 0; AccS >= 0.01 && k < 12; k++) {
    LastG = AP->Run(st);
    FBControls c = FC->Run(LastG, st);
    fb_jsbsim_set_controls(c.Roll, c.Pitch, c.Yaw, c.Thr);
    fb_jsbsim_step(&st);
    AccS -= 0.01;
    LastSub++;
  }
}

/* Applies only the fields FBPilotCommands actually set (Guidance != None, each std::optional present) —
 * an Idle-phase neutral FBPilotCommands (every field default/unset) reaches here and calls NOTHING,
 * which is what keeps an un-started pilot (Phase::Idle) bit-identical to not having one (see Run()'s
 * banner). */
void FBF16Module::ApplyPilotCommands(const FBPilotCommands &c) {
  constexpr double kKtToMs = 0.5144444444;
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

/* Boundary input (mission-file text, defensive checks per CLAUDE.md's C++ conventions) — a value that
 * doesn't parse as a number is treated as 0.0 rather than crashing/throwing on this system-boundary. */
namespace {
double ParseDouble(const std::string &s) {
  char *end = nullptr;
  double v = std::strtod(s.c_str(), &end);
  return (end && end != s.c_str()) ? v : 0.0;
}
} // namespace

bool FBF16Module::ApplySetup(const std::string &key, const std::string &value) {
  if (key == "gear") {
    if (value != "up" && value != "down") return false;
    AirframeCtrl->SetGear(value == "down");
    return true;
  }
  if (key == "fuel_lbs") {
    double lbs = ParseDouble(value);
    if (lbs < 0.0) lbs = 0.0;
    fb_jsbsim_set_fuel_total_lbs(lbs);
    return true;
  }
  if (key == "fuel_pct") {
    double pct = ParseDouble(value);
    if (pct < 0.0) pct = 0.0; else if (pct > 100.0) pct = 100.0;
    fb_jsbsim_set_fuel_pct(pct);
    return true;
  }
  return false;
}

} // namespace FlightBox
