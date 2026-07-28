#include "FBMig29Module.h"
#include "FBLog.h"
#include "FBUnits.h"
#include <cerrno>
#include <cmath>
#include <cstdlib>

namespace FlightBox::Modules {

FBMig29Module::FBMig29Module()
    : AP(std::make_unique<Systems::FBAutopilot>()),
      FC(std::make_unique<Systems::FBFlightControl>(Systems::FBFlightControl::Mig29())),
      Disp(std::make_unique<Systems::FBDisplaySystem>()),   /* the generic MIL-STD-1787 default: this
                                                             * aircraft's own symbology is stage 2b */
      AirData(std::make_unique<Systems::FBAirDataSystem>()),
      RadarAlt(std::make_unique<Systems::FBRadarAltimeter>()),
      NavSys(std::make_unique<Systems::FBNavSystem>()),
      Warn_(std::make_unique<Systems::FBWarningSystem>()),
      PilotSys(std::make_unique<FBMig29Pilot>()),
      AirframeCtrl(std::make_unique<Systems::FBAirframeControls>()) {}

void FBMig29Module::AttachFdm(Fdm::FBFdm &fdm) {
  Fdm_ = &fdm;
  AirframeCtrl = std::make_unique<Systems::FBJsbsimAirframeControls>(fdm);
}

bool FBMig29Module::Due(double &accS, double dt, double hz) {
  accS += dt;
  double period = 1.0 / hz;
  if (accS < period) return false;
  accS -= period;
  return true;
}

/* Same shape as FBF16Module::Run and deliberately so — sensor group, displays, pilot, then the fixed
 * 100 Hz FDM substeps with the same spiral guard. Shorter only because there are fewer slots to cycle. */
void FBMig29Module::Run(Fdm::fb_fdm_state &st, double dt, const Units::FBUnitRegistry *units,
                        const World::FBWorld *world) {
  (void)units;   /* no sensor slot is cycled yet — the registry reaches nothing here (stage 2b) */
  (void)world;
  if (!Fdm_) return;
  Fdm::FBFdm &fdm = *Fdm_;
  SimTimeS += dt;
  SharedState.NowS = SimTimeS;
  CmdBus_.SetLoadFactor(SharedState.AirData.H.Readable() ? SharedState.AirData.GLoad : 1.0f);

  if (Due(SensorAccS, dt, 10.0)) {
    PublishPlatform(st);
    PublishAirframe();
    AirData->Run(SharedState, st, dt);
    RadarAlt->Run(SharedState, (float)st.elev, GroundAslM);
    if (const FBWaypoint *swp = Plan_.ActiveWaypoint())
      NavSys->SetSteerpoint(swp->LatDeg, swp->LonDeg, GroundAslM * kMToFt);
    NavSys->Run(SharedState, st, dt);
    Warn_->Run(SharedState, dt);   /* LAST: a pure consumer of everything above it */
  }
  if (Due(DisplayAccS, dt, 20.0)) Disp->Run(SharedState, Mode, dt);

  if (Due(PilotAccS, dt, 10.0)) {
    ApplyPilotCommands(PilotSys->Run(SharedState, CmdBus_, *AirframeCtrl, st, Plan_,
                                     HaveRunway_ ? &Rwy_ : nullptr, dt));
    RunConfiguration();
    NavSys->AdvanceWaypoint(Plan_, st.lat, st.lon);
  }

  AccS += dt;
  LastSub = 0;
  for (int k = 0; AccS >= Fdm::FBFdm::kStepS && k < 12; k++) {
    LastG = AP->Run(st);
    Systems::FBControls c = FC->Run(LastG, st);
    fdm.SetControls(c.Roll, c.Pitch, c.Yaw, c.Thr);
    fdm.Step(st);
    AccS -= Fdm::FBFdm::kStepS;
    LastSub++;
  }
}

/* THE CONFIGURATION SCHEDULE — this aircraft's flaps and slats, on the pilot's own decision tick.
 *
 * WHY IT IS IN THE MODULE. systems/FBAirframeControls has no flap channel, and the F-16 (the only other
 * airframe here) has none to command: its leading- and trailing-edge surfaces are scheduled inside its
 * FLCS. Adding a flap command to the generic pilot would mean inventing a generic flap SCHEDULE for
 * airframes that do not have flaps. So the schedule lives with the aircraft that has them, reads the
 * generic pilot's published phase, and writes fcs/flap-cmd-norm — which the deck couples to the slats
 * ([DCS-EA p.57]: either DOWN button extends both).
 *
 * THE SCHEDULE IS THE DOCUMENTED ONE (doc/mig29/procedures.md §1):
 *   down before takeoff, retract at 350 ft, extend again on the downwind leg.
 * The retraction is LATCHED at 350 ft rather than keyed to a phase, because FBPilot leaves Climb when
 * the gear is up, which on this jet happens well below 350 ft — and an unlatched altitude rule would
 * drop the flaps again on any low-level route leg. */
void FBMig29Module::RunConfiguration() {
  using Phase = Pilot::FBPilot::Phase;
  const Phase p = PilotSys->GetPhase();
  double want = FlapCmd_;
  switch (p) {
    case Phase::Idle:
    case Phase::Shutdown:
      return;                                  /* nobody is flying it: leave the surfaces alone */
    case Phase::Preflight:
    case Phase::Takeoff:
      want = 1.0;
      FlapRetracted_ = false;
      break;
    case Phase::Approach:
    case Phase::Flare:
    case Phase::Rollout:
      want = 1.0;                              /* [DOC] extended on the downwind leg, and stay out */
      FlapRetracted_ = false;
      break;
    default:
      /* Airborne and not landing. The AGL gate asks the block head first, exactly as every one of
       * FBPilot's own AGL gates does: without a valid radar altitude the configuration is not changed. */
      if (!FlapRetracted_ && SharedState.RadarAlt.H.Readable() &&
          SharedState.RadarAlt.AglFt > kFlapRetractAglFt)
        FlapRetracted_ = true;
      want = FlapRetracted_ ? 0.0 : 1.0;
      break;
  }
  if (want == FlapCmd_) return;                /* one write per change, not one per tick */
  FlapCmd_ = want;
  if (Fdm_) Fdm_->SetFlap(want);
  FBLog::Info("mig29", "CONFIG", {{"flapCmd", want}, {"phase", Pilot::FBPilot::PhaseName(p)},
      {"aglFt", (double)SharedState.RadarAlt.AglFt}});
}

void FBMig29Module::PublishPlatform(const Fdm::fb_fdm_state &st) {
  FBPlatformBlock &b = SharedState.Platform;
  b.RollDeg = (float)st.roll; b.PitchDeg = (float)st.pitch; b.YawDeg = (float)st.yaw;
  b.AltM = (float)st.elev;
  b.GsMs = (float)st.gs; b.TasMs = (float)st.speed; b.VsMs = (float)st.vy;
  b.Mode = AP->GetMode();
  b.H.Publish(SharedState.NowS);
}

void FBMig29Module::PublishAirframe() {
  FBAirframeBlock &b = SharedState.Airframe;
  b.GearPosition = (float)AirframeCtrl->GetGearPosition();
  b.WeightOnWheels = AirframeCtrl->GetWeightOnWheels();
  b.SpeedbrakeNorm = (float)AirframeCtrl->GetSpeedbrake();
  b.EngineRunning = AirframeCtrl->GetEngineRunning(0);
  if (Fdm_) {
    double cap = Fdm_->GetFuelCapacityLbs();
    b.FuelLbs = (float)Fdm_->GetFuelTotalLbs();
    b.FuelPct = cap > 0.0 ? (float)(100.0 * b.FuelLbs / cap) : 0.0f;
  }
  b.H.Publish(SharedState.NowS);
}

void FBMig29Module::ApplyPilotCommands(const Pilot::FBPilotCommands &c) {
  switch (c.Guidance) {
    case Pilot::FBPilotGuidance::Manual:
      AP->SetManual(c.ManualRoll, c.ManualPitch, c.ManualYaw, c.ManualThr);
      break;
    case Pilot::FBPilotGuidance::Direct:
      if (c.HaveLeg)
        AP->SetDirectLeg(c.LegLatDeg, c.LegLonDeg, c.TargetLatDeg, c.TargetLonDeg, c.TargetAltM,
                         c.TargetSpeedKt * kKtToMs);
      else
        AP->SetDirect(c.TargetLatDeg, c.TargetLonDeg, c.TargetAltM, c.TargetSpeedKt * kKtToMs);
      break;
    case Pilot::FBPilotGuidance::Course:
      AP->SetCourse(c.TargetLatDeg, c.TargetLonDeg, c.CourseDeg, c.TargetAltM, c.GlidepathDeg,
                    c.TargetSpeedKt * kKtToMs);
      break;
    case Pilot::FBPilotGuidance::None:
      break;
  }
  if (c.GearDown) AirframeCtrl->SetGear(*c.GearDown);
  if (c.Speedbrake) AirframeCtrl->SetSpeedbrake(*c.Speedbrake);
  if (c.WheelBrakeLeft || c.WheelBrakeRight)
    AirframeCtrl->SetWheelBrakes(c.WheelBrakeLeft.value_or(0.0), c.WheelBrakeRight.value_or(0.0));
  if (c.NosewheelSteer) AirframeCtrl->SetNosewheelSteer(*c.NosewheelSteer);
  if (c.EngineStart) *c.EngineStart ? AirframeCtrl->EngineStart() : AirframeCtrl->EngineCutoff();
}

namespace {
/* Boundary input, strict for the same reason as the F-16's: a silent 0.0 would spawn the jet with an
 * empty tank and report success. */
bool ParseDouble(const std::string &s, double &out) {
  if (s.empty()) return false;
  char *end = nullptr;
  errno = 0;
  double v = std::strtod(s.c_str(), &end);
  if (end != s.c_str() + s.size()) return false;
  if (errno == ERANGE || !std::isfinite(v)) return false;
  out = v;
  return true;
}

bool RejectSetup(const char *reason, const std::string &key, const std::string &value) {
  FBLog::Error("module", "SET_INVALID_VALUE", {{"key", key}, {"value", value}, {"reason", reason}});
  return false;
}
} // namespace

/* DELIBERATELY SHORT. A key here is a state this airframe can actually take; every avionics key the
 * F-16 answers (fcr_*, rwr_*, cmds_*, datalink*, brief_*, store) names a box this module does not
 * compose, and answering it would report success for a setting with no system behind it. Unknown key =
 * mission FAIL, which is what the caller does with the false. */
bool FBMig29Module::ApplySetup(const std::string &key, const std::string &value) {
  if (!Fdm_) return false;
  if (key == "gear") {
    if (value != "up" && value != "down") return RejectSetup("want up|down", key, value);
    AirframeCtrl->SetGear(value == "down");
    return true;
  }
  if (key == "task") {
    /* Only `route` this round: bfm/intercept/attack all need a sensor or a weapon this module has not
     * got, and a phase that reads an Invalid block forever is a jet that flies straight ahead. */
    if (value != "route") return RejectSetup("want route (bfm/intercept/attack need stage 2b/2c)",
                                             key, value);
    PilotSys->SetPhase(Pilot::FBPilot::Phase::Route);
    return true;
  }
  if (key == "radalt") {
    if (value != "on" && value != "off") return RejectSetup("want on|off", key, value);
    RadarAlt->SetPowered(value == "on");
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
  return false;
}

} // namespace FlightBox::Modules
