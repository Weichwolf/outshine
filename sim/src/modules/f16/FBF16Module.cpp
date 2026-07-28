#include "FBF16Module.h"
#include "FBF16Hud.h"
#include "FBLog.h"
#include "FBUnits.h"
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace FlightBox::Modules {

FBF16Module::FBF16Module()
    : AP(std::make_unique<Systems::FBAutopilot>()),
      FC(std::make_unique<Systems::FBFlightControl>(Systems::FBFlightControl::F16())),
      Input(std::make_unique<Systems::FBInputSystem>()),
      Propulsion(std::make_unique<Systems::FBPropulsionSystem>()),
      Disp(std::make_unique<FBF16Hud>()),
      Chip(std::make_unique<FBF16Max7456>()),
      Fcr_(std::make_unique<FBF16Fcr>()),
      Weapons(std::make_unique<Systems::FBWeaponSystem>()),
      Rwr_(std::make_unique<FBF16Rwr>()),
      Cmds_(std::make_unique<FBF16Cmds>()),
      Datalink_(std::make_unique<FBF16Datalink>()),
      AirData(std::make_unique<Systems::FBAirDataSystem>()),
      RadarAlt(std::make_unique<Systems::FBRadarAltimeter>()),
      NavSys(std::make_unique<Systems::FBNavSystem>()),
      FireCtrl(std::make_unique<FBF16FireControl>()),
      UfcSys(std::make_unique<FBF16Ufc>()),
      SmsSys(std::make_unique<FBF16Sms>()),
      GunSys(std::make_unique<FBF16Gun>()),
      Warn_(std::make_unique<Systems::FBWarningSystem>()),
      PilotSys(std::make_unique<FBF16Pilot>()),
      AirframeCtrl(std::make_unique<Systems::FBAirframeControls>()) {}   /* NoOp until an airframe is attached */

void FBF16Module::AttachFdm(Fdm::FBFdm &fdm) {
  Fdm_ = &fdm;
  AirframeCtrl = std::make_unique<Systems::FBJsbsimAirframeControls>(fdm);
  /* The pylons become real point masses on THIS airframe — every station empty until a mission loads
   * one, so an unloaded jet is unchanged. */
  SmsSys->AttachFdm(fdm);
}

bool FBF16Module::Due(double &accS, double dt, double hz) {
  accS += dt;
  double period = 1.0 / hz;
  if (accS < period) return false;
  accS -= period;
  return true;
}

/* Cycles every system slot, then the fixed 100 Hz FDM substeps (spiral guard, <=12/frame): guidance ->
 * FLCS-command -> JSBSim in lockstep. AP->Run()/FC->Run() are the only virtual dispatch INSIDE that
 * inner loop; every other slot is throttled outside it. Rate table: doc/flightbox/modules-f16.md §2.2. */
void FBF16Module::Run(Fdm::fb_fdm_state &st, double dt, const Units::FBUnitRegistry *units, const World::FBWorld *world) {
  if (!Fdm_) return;               /* no airframe attached — nothing to fly */
  Fdm::FBFdm &fdm = *Fdm_;
  SimTimeS += dt;
  SharedState.NowS = SimTimeS;     /* the bus time every block header is stamped against */
  /* The DED gate: unpublished air data reads as 1 g — a jet with no ADC is not manoeuvring by any
   * measurement this aircraft has. */
  CmdBus_.SetLoadFactor(SharedState.AirData.H.Readable() ? SharedState.AirData.GLoad : 1.0f);
  Input->Run(Mode, dt);
  Propulsion->Run(st, dt);

  if (Due(SensorAccS, dt, 10.0)) {
    /* The module's own two blocks FIRST, because the systems below read them — that is what keeps "one
     * writer per block" true instead of each consumer reaching for `st`. */
    PublishPlatform(st);
    PublishAirframe();
    /* Commands before the boxes they address, so a switch the pilot threw takes effect on the next
     * sweep and not the one after it. */
    ServiceCommands(FBCommandGroup::Sensors);
    ServiceCommands(FBCommandGroup::Avionics);
    ServiceCommands(FBCommandGroup::Stores);
    /* THE DAMAGE PATTERN, identical at every box below (doc/flightbox/modules-f16.md §2.4): a FAILED
     * system is not cycled at all and its block goes Invalid, so every consumer behaves as it already
     * does for a box that was never powered; a DEGRADED one runs at its one derivable restriction. */
    Fcr_->SetRangeFactor(SystemDegraded(FBSystemId::Radar) ? kRadarRangeDegraded : 1.0);
    if (SystemWorking(FBSystemId::Radar)) Fcr_->Run(SharedState, st, units, SimTimeS);
    else SharedState.Radar.H.Invalidate();
    /* One throttle group so FireControl always reads Nav's SAME-tick output; `st` is the FDM state as
     * of the END of the previous Run(), the one-tick lag every Sensor-cadence write has. */
    if (SystemWorking(FBSystemId::AirData)) AirData->Run(SharedState, st, dt);
    else SharedState.AirData.H.Invalidate();
    if (SystemWorking(FBSystemId::RadarAlt)) RadarAlt->Run(SharedState, (float)st.elev, GroundAslM);
    else SharedState.RadarAlt.H.Invalidate();
    /* Republished every sensor tick because the active waypoint advances during the run. A `wp` line
     * declares the altitude to FLY, not the terrain under it, so the steerpoint's ground elevation is
     * the only terrain figure the module has: this tick's sample under the aircraft. */
    if (const FBWaypoint *swp = Plan_.ActiveWaypoint())
      NavSys->SetSteerpoint(swp->LatDeg, swp->LonDeg, GroundAslM * kMToFt);
    if (SystemWorking(FBSystemId::Nav)) {
      NavSys->Run(SharedState, st, dt);
    } else {
      SharedState.Nav.H.Invalidate();
      SharedState.Cruise.H.Invalidate();   /* the same box publishes both messages */
    }
    /* The SELECTED station's round: the launch zone computed is for the weapon that would actually
     * leave the jet if the pilot pickled now. */
    if (SystemWorking(FBSystemId::FireControl))
      FireCtrl->Run(SharedState, st, FBStoreSpecOf(SmsSys->StoreAt(SmsSys->SelectedStation())),
                    GunSys->Spec(), SimTimeS, dt);
    else SharedState.FireControl.H.Invalidate();
    /* What a launched round carries out of the jet: a guided one its target estimate, an unguided one
     * the delivery solution it was aimed with. */
    SmsSys->SetTargetState(FireCtrl->TargetState());
    SmsSys->SetReleaseSolution(FireCtrl->ReleaseSolution());
    UfcSys->Run(SharedState, dt);
    if (SystemWorking(FBSystemId::Stores)) SmsSys->Run(SharedState, dt);
    else SharedState.Stores.H.Invalidate();
    /* LAST: a pure consumer of everything above it, including their validity heads. */
    Warn_->Run(SharedState, dt);
  }
  if (Due(DisplayAccS, dt, 20.0)) Disp->Run(SharedState, Mode, dt);
  if (Due(WeaponAccS, dt, 20.0)) Weapons->Run(Mode, world, dt);
  /* THE GUN, once per Run() with the FULL dt — deliberately unthrottled: its output is a round count
   * INTEGRATED over time, so any other cadence would drop rounds or invent them. */
  if (SystemWorking(FBSystemId::Gun)) GunSys->Run(SharedState, st, SimTimeS, dt);
  else SharedState.Gun.H.Invalidate();
  /* Defensive, in data-flow order: the receiver writes the threat picture, the dispenser reads it, and
   * the CMS commands are answered in between so a throw hits this tick's program and not the next. */
  if (Due(DefensiveAccS, dt, 10.0)) {
    if (SystemWorking(FBSystemId::Rwr)) Rwr_->Run(SharedState, st, units, SimTimeS);
    else SharedState.Rwr.H.Invalidate();
    ServiceCommands(FBCommandGroup::Defensive);
    if (SystemWorking(FBSystemId::Countermeasures)) Cmds_->Run(SharedState, st, SimTimeS);
    else SharedState.Cmds.H.Invalidate();
  }
  /* Comms/Datalink: the COOPERATIVE half. `units` reaches this slot and the Sensors slot above, and
   * nothing else; what leaves is instrument data in SharedState. */
  if (Due(CommsAccS, dt, 5.0)) {
    ServiceCommands(FBCommandGroup::Comms);
    if (SystemWorking(FBSystemId::Datalink)) Datalink_->Run(SharedState, st, units, SimTimeS);
    else SharedState.Datalink.H.Invalidate();
  }
  /* Pilot. Waypoint capture runs right after, same cadence: THIS tick's Run() flew toward the
   * pre-capture active waypoint. Whether the MISSION concluded from that same capture is a separate,
   * independent judgement the caller owns. */
  if (Due(PilotAccS, dt, 10.0)) {
    ApplyPilotCommands(PilotSys->Run(SharedState, CmdBus_, *AirframeCtrl, st, Plan_,
                                     HaveRunway_ ? &Rwy_ : nullptr, dt));
    /* Forwarded like the platform block: the writer is one system, it just does not hold the bus. */
    SharedState.Bfm = PilotSys->BfmTrack().Block();
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

void FBF16Module::PublishPlatform(const Fdm::fb_fdm_state &st) {
  FBPlatformBlock &b = SharedState.Platform;
  b.RollDeg = (float)st.roll; b.PitchDeg = (float)st.pitch; b.YawDeg = (float)st.yaw;
  b.AltM = (float)st.elev;
  b.GsMs = (float)st.gs; b.TasMs = (float)st.speed; b.VsMs = (float)st.vy;
  b.Mode = AP->GetMode();
  b.H.Publish(SharedState.NowS);
}

void FBF16Module::PublishAirframe() {
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


/* The MODULE routes for the same reason it interprets its own `set` keys: the systems are generic, and
 * which F-16 box owns "radar mode" is this aircraft's knowledge, not systems/'s. */
void FBF16Module::ServiceCommands(FBCommandGroup group) {
  FBAvionicsCommand c{};
  while (CmdBus_.TakeDue(group, SimTimeS, c)) {
    FBCommandOutcome outcome = FBCommandOutcome::Accepted;
    FBCommandReason reason = FBCommandReason::None;
    ApplyCommand(c, outcome, reason);
    CmdBus_.Complete(c, outcome, reason, SimTimeS);
  }
}

/* Where the RANGE POLICY lives, next to the box that knows the domain: out of range is REJECTED and
 * named, never clamped behind the pilot's back. The one clamp is the documented BNGO ceiling, and it is
 * reported as Clamped rather than as success. Table: doc/flightbox/modules-f16.md §2.5. */
namespace {
/* False for a command whose owner is not a system the damage model tracks (UFC entry, master mode). */
bool CommandOwner(FBCommandTarget t, FBSystemId &out) {
  switch (t) {
    case FBCommandTarget::RadarMode:
    case FBCommandTarget::RadarRangeNm:
    case FBCommandTarget::RadarSlewAz:
    case FBCommandTarget::RadarSlewEl:
    case FBCommandTarget::IffTransponder:
    case FBCommandTarget::IffInterrogator:
    case FBCommandTarget::Designate: out = FBSystemId::Radar; return true;
    case FBCommandTarget::DatalinkPower:
    case FBCommandTarget::DatalinkTransmit:
    case FBCommandTarget::DatalinkFilter:
    case FBCommandTarget::DatalinkRangeNm: out = FBSystemId::Datalink; return true;
    case FBCommandTarget::MasterArm:
    case FBCommandTarget::StationSelect:
    case FBCommandTarget::WeaponSelect:
    case FBCommandTarget::WeaponRelease: out = FBSystemId::Stores; return true;
    case FBCommandTarget::CmDispense:
    case FBCommandTarget::CmConsent:
    case FBCommandTarget::CmdsMode: out = FBSystemId::Countermeasures; return true;
    case FBCommandTarget::GunTrigger: out = FBSystemId::Gun; return true;
    default: return false;
  }
}
} // namespace

void FBF16Module::ApplyCommand(const FBAvionicsCommand &c, FBCommandOutcome &outcome,
                               FBCommandReason &reason) {
  /* THE DAMAGE GATE, before any box is touched: without it a destroyed SMS would still let a round off
   * the rail, because the release path runs through this router and not through SmsSys->Run(). */
  FBSystemId owner = FBSystemId::Engine;
  if (CommandOwner(c.Target, owner) && !SystemWorking(owner)) {
    outcome = FBCommandOutcome::Rejected;
    reason = FBCommandReason::SystemFailed;
    return;
  }
  switch (c.Target) {
    case FBCommandTarget::RadarMode: {
      int ord = (int)c.Value;
      if (ord < 0 || ord > (int)FBF16FcrMode::AcmSlew) { outcome = FBCommandOutcome::Rejected; reason = FBCommandReason::OutOfRange; return; }
      Fcr_->SetMode((FBF16FcrMode)ord);
      return;
    }
    case FBCommandTarget::RadarRangeNm:
      if (!(c.Value > 0.0) || c.Value > 160.0) { outcome = FBCommandOutcome::Rejected; reason = FBCommandReason::OutOfRange; return; }
      Fcr_->SetRangeOverrideNm(c.Value);
      return;
    case FBCommandTarget::RadarSlewAz:
    case FBCommandTarget::RadarSlewEl:
      if (c.Value < -FBF16Fcr::kGimbalAzDeg || c.Value > FBF16Fcr::kGimbalAzDeg) { outcome = FBCommandOutcome::Rejected; reason = FBCommandReason::OutOfRange; return; }
      if (c.Target == FBCommandTarget::RadarSlewAz) Fcr_->SetSlewAz(c.Value);
      else Fcr_->SetSlewEl(c.Value);
      return;
    case FBCommandTarget::IffTransponder: Fcr_->SetIffTransponder(c.Value != 0.0); return;
    case FBCommandTarget::IffInterrogator: Fcr_->SetIffInterrogator(c.Value != 0.0); return;
    case FBCommandTarget::DatalinkPower: Datalink_->SetPowered(c.Value != 0.0); return;
    case FBCommandTarget::DatalinkTransmit: Datalink_->SetTransmit(c.Value != 0.0); return;
    case FBCommandTarget::DatalinkFilter: {
      int ord = (int)c.Value;
      if (ord < 0 || ord > (int)FBF16ContactFilter::Off) { outcome = FBCommandOutcome::Rejected; reason = FBCommandReason::OutOfRange; return; }
      Datalink_->SetContactFilter((FBF16ContactFilter)ord);
      return;
    }
    case FBCommandTarget::DatalinkRangeNm:
      if (!(c.Value > 0.0) || c.Value > 500.0) { outcome = FBCommandOutcome::Rejected; reason = FBCommandReason::OutOfRange; return; }
      Datalink_->SetMaxRangeM(c.Value * kNmToM);
      return;
    case FBCommandTarget::MasterMode: {
      int ord = (int)c.Value;
      if (ord < 0 || ord > (int)FBMasterMode::Dogfight) { outcome = FBCommandOutcome::Rejected; reason = FBCommandReason::OutOfRange; return; }
      Mode = (FBMasterMode)ord;
      return;
    }
    case FBCommandTarget::MasterArm:
      /* ONE switch, both weapon systems: master arm is a cockpit control, not an SMS property, and a
       * jet whose racks are armed has an armed gun too. */
      SmsSys->SetMasterArm(c.Value != 0.0 ? FBArmState::Arm : FBArmState::Sim);
      GunSys->SetMasterArm(SmsSys->MasterArm());
      return;
    case FBCommandTarget::StationSelect:
      if (!SmsSys->SelectStation((int)c.Value)) {
        outcome = FBCommandOutcome::Rejected;
        reason = FBCommandReason::OutOfContext;
      }
      return;
    /* Release/trigger/dispense: the owning BOX decides and says why; this module only routes. */
    case FBCommandTarget::WeaponRelease:
      SmsSys->Release(SimTimeS, outcome, reason);
      return;
    case FBCommandTarget::GunTrigger:
      GunSys->Trigger(c.Value, SimTimeS, outcome, reason);
      return;
    case FBCommandTarget::CmDispense:
      if (c.Value < 0.0 || c.Value > (double)Sensors::FBCountermeasureSystem::kProgramCount) {
        outcome = FBCommandOutcome::Rejected;
        reason = FBCommandReason::OutOfRange;
        return;
      }
      Cmds_->Dispense((int)c.Value, SimTimeS, outcome, reason);
      return;
    case FBCommandTarget::CmConsent:
      Cmds_->SetConsent(c.Value != 0.0);
      return;
    case FBCommandTarget::CmdsMode: {
      int ord = (int)c.Value;
      if (ord < 0 || ord > (int)FBCmdsMode::Byp) {
        outcome = FBCommandOutcome::Rejected;
        reason = FBCommandReason::OutOfRange;
        return;
      }
      Cmds_->SetMode((FBCmdsMode)ord);
      return;
    }
    case FBCommandTarget::AlowFt:
      if (!FBF16Ufc::AlowInRange((float)c.Value)) { outcome = FBCommandOutcome::Rejected; reason = FBCommandReason::OutOfRange; return; }
      UfcSys->SetAlow((float)c.Value);
      /* §6.4: the entry commits either way, but with the CARA unpowered the warning it arms can never
       * fire — and the pilot is told instead of assuming a live floor. */
      if (!RadarAlt->Powered()) { outcome = FBCommandOutcome::Inhibited; reason = FBCommandReason::EffectPrecondition; }
      return;
    case FBCommandTarget::BingoLbs:
      if (!FBF16Ufc::BingoInRange((float)c.Value)) { outcome = FBCommandOutcome::Rejected; reason = FBCommandReason::OutOfRange; return; }
      /* §6.8: ENTR succeeds and the field shows what was typed, but the warning fires at the system
       * ceiling — accepted-with-clamp, not a rejection. */
      if (!UfcSys->SetBingo((float)c.Value)) { outcome = FBCommandOutcome::Clamped; reason = FBCommandReason::ValueClamped; }
      return;
    case FBCommandTarget::SteerpointNum:
      if (!FBF16Ufc::SteerNumInRange((int)c.Value)) { outcome = FBCommandOutcome::Rejected; reason = FBCommandReason::OutOfRange; return; }
      UfcSys->SetSteerpointNumber((int)c.Value);
      return;
    /* Value = the contact's published track number, 0 = break lock. A number naming no firm track is
     * OutOfContext, not a range error: the return is gone by the time the hand finished moving. */
    case FBCommandTarget::Designate:
      if (!Fcr_->Designate((int)c.Value, SimTimeS)) {
        outcome = FBCommandOutcome::Rejected;
        reason = FBCommandReason::OutOfContext;
      }
      return;
    /* §6.6: the F-16 has it, FlightBox has not — a silent success would be a lie the pilot flies on. */
    case FBCommandTarget::WeaponSelect:
      outcome = FBCommandOutcome::Rejected;
      reason = FBCommandReason::NotImplemented;
      return;
    case FBCommandTarget::None:
      outcome = FBCommandOutcome::Rejected;
      reason = FBCommandReason::OutOfContext;
      return;
  }
}

/* Only the fields actually SET — which is what keeps an un-started pilot (Phase::Idle, everything
 * default) bit-identical to not having one. */
void FBF16Module::ApplyPilotCommands(const Pilot::FBPilotCommands &c) {
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
      break;   /* leave whatever guidance is already running untouched */
  }
  if (c.GearDown) AirframeCtrl->SetGear(*c.GearDown);
  if (c.Speedbrake) AirframeCtrl->SetSpeedbrake(*c.Speedbrake);
  if (c.WheelBrakeLeft || c.WheelBrakeRight)
    AirframeCtrl->SetWheelBrakes(c.WheelBrakeLeft.value_or(0.0), c.WheelBrakeRight.value_or(0.0));
  if (c.NosewheelSteer) AirframeCtrl->SetNosewheelSteer(*c.NosewheelSteer);
  if (c.EngineStart) *c.EngineStart ? AirframeCtrl->EngineStart() : AirframeCtrl->EngineCutoff();
}

/* Boundary input. STRICT: the whole value must be one finite number and nothing else — a silent 0.0
 * would spawn the jet with an empty tank, report success, and kill the engine minutes later with
 * nothing in events.log naming the cause. */
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

/* The only place the raw value is still known: the caller turns the false into a mission FAIL. */
bool RejectSetup(const char *reason, const std::string &key, const std::string &value) {
  FBLog::Error("module", "SET_INVALID_VALUE", {{"key", key}, {"value", value}, {"reason", reason}});
  return false;
}
} // namespace

bool FBF16Module::ApplySetup(const std::string &key, const std::string &value) {
  if (!Fdm_) return false;   /* setup lines describe the airframe's state — none to apply without one */
  /* POWER and XMT are two switches because the real terminal has two: power off blinds this jet, XMT
   * off only stops it being heard. */
  if (key == "datalink" || key == "datalink_xmt") {
    if (value != "on" && value != "off") return RejectSetup("want on|off", key, value);
    if (key == "datalink") Datalink_->SetPowered(value == "on");
    else Datalink_->SetTransmit(value == "on");
    return true;
  }
  if (key == "datalink_filter") {
    FBF16ContactFilter f;
    if (!FBF16ContactFilterFromString(value.c_str(), f)) return RejectSetup("want fr|fl|off", key, value);
    Datalink_->SetContactFilter(f);
    return true;
  }
  if (key == "datalink_range_nm") {
    double nm = 0.0;
    if (!ParseDouble(value, nm)) return RejectSetup("not a number", key, value);
    if (nm <= 0.0) return RejectSetup("range must be positive", key, value);
    Datalink_->SetMaxRangeM(nm * kNmToM);
    return true;
  }
  /* The two halves of the APX-113 are separately switchable because they answer different questions:
   * whether OTHERS can identify this jet, and whether this jet can identify others. */
  if (key == "fcr_mode") {
    FBF16FcrMode m;
    if (!FBF16FcrModeFromString(value.c_str(), m))
      return RejectSetup("want off|crm|acm_hud|acm_bore|acm_vert|acm_slew", key, value);
    Fcr_->SetMode(m);
    return true;
  }
  if (key == "fcr_range_nm") {
    double nm = 0.0;
    if (!ParseDouble(value, nm)) return RejectSetup("not a number", key, value);
    if (nm <= 0.0) return RejectSetup("range must be positive", key, value);
    Fcr_->SetRangeOverrideNm(nm);
    return true;
  }
  if (key == "fcr_slew_az" || key == "fcr_slew_el") {
    double deg = 0.0;
    if (!ParseDouble(value, deg)) return RejectSetup("not a number", key, value);
    if (deg < -FBF16Fcr::kGimbalAzDeg || deg > FBF16Fcr::kGimbalAzDeg)
      return RejectSetup("outside the antenna gimbal limits", key, value);
    if (key == "fcr_slew_az") Fcr_->SetSlewAz(deg);
    else Fcr_->SetSlewEl(deg);
    return true;
  }
  if (key == "iff_xpdr" || key == "iff_interrogator") {
    if (value != "on" && value != "off") return RejectSetup("want on|off", key, value);
    if (key == "iff_xpdr") Fcr_->SetIffTransponder(value == "on");
    else Fcr_->SetIffInterrogator(value == "on");
    return true;
  }
  /* Which phase the pilot starts in. Declared as mission data rather than inferred from the loadout:
   * two jets with identical setup lines can have opposite jobs. */
  if (key == "task") {
    if (value == "route") PilotSys->SetPhase(Pilot::FBPilot::Phase::Route);
    else if (value == "bfm") PilotSys->SetPhase(Pilot::FBPilot::Phase::Bfm);
    else if (value == "intercept") PilotSys->SetPhase(Pilot::FBPilot::Phase::Intercept);
    else if (value == "attack") PilotSys->SetPhase(Pilot::FBPilot::Phase::Attack);
    else return RejectSetup("want route|bfm|intercept|attack", key, value);
    return true;
  }
  /* ONE line, TWO consumers — the pilot needs the cue to release on, the fire control needs it for the
   * release record. Neither changes the arithmetic: both cues come out of the same integration. */
  if (key == "attack_mode") {
    FBDeliveryMode m;
    if (value == "ccip") m = FBDeliveryMode::Ccip;
    else if (value == "ccrp") m = FBDeliveryMode::Ccrp;
    else return RejectSetup("want ccip|ccrp", key, value);
    PilotSys->BriefAttack(m);
    FireCtrl->SetDeliveryMode(m);
    return true;
  }
  /* A mission may load LESS than the drum holds (a jet that has been shooting, or the empty-magazine
   * case a refusal must be proven against) and never more. */
  if (key == "gun_rounds") {
    double n = 0.0;
    if (!ParseDouble(value, n)) return RejectSetup("not a number", key, value);
    if (n < 0.0 || n != std::floor(n)) return RejectSetup("want a whole, non-negative round count", key, value);
    if (!GunSys->SetRounds((int)n)) return RejectSetup("more rounds than the gun holds", key, value);
    return true;
  }
  if (key == "rwr" || key == "rwr_search") {
    if (value != "on" && value != "off") return RejectSetup("want on|off", key, value);
    if (key == "rwr") Rwr_->SetPowered(value == "on");
    else Rwr_->SetSearchShown(value == "on");
    return true;
  }
  if (key == "rwr_display") {
    FBF16RwrDisplay d;
    if (!FBF16RwrDisplayFromString(value.c_str(), d))
      return RejectSetup("want priority|open", key, value);
    Rwr_->SetDisplay(d);
    return true;
  }
  if (key == "cmds_mode") {
    FBCmdsMode m;
    if (!FBCmdsModeFromString(value.c_str(), m))
      return RejectSetup("want off|stby|man|semi|auto|byp", key, value);
    Cmds_->SetMode(m);
    return true;
  }
  if (key == "cmds_program") {
    double n = 0.0;
    if (!ParseDouble(value, n)) return RejectSetup("not a number", key, value);
    if (!Cmds_->SelectProgram((int)n)) return RejectSetup("no such program (1..6)", key, value);
    return true;
  }
  if (key == "cmds_chaff" || key == "cmds_flare") {
    double n = 0.0;
    if (!ParseDouble(value, n)) return RejectSetup("not a number", key, value);
    if (n < 0.0 || n > (double)FBF16Cmds::kMaxCombined)
      return RejectSetup("outside the dispenser's capacity", key, value);
    int chaff = key == "cmds_chaff" ? (int)n : Cmds_->ChaffRemaining();
    int flare = key == "cmds_flare" ? (int)n : Cmds_->FlareRemaining();
    if (chaff + flare > FBF16Cmds::kMaxCombined)
      return RejectSetup("chaff + flare exceeds 120 combined", key, value);
    Cmds_->SetLoadout(chaff, flare);
    return true;
  }
  /* The one mission-declarable way to make a source block INVALID, which is what a consumer's handling
   * of that state can be measured against. */
  if (key == "radalt") {
    if (value != "on" && value != "off") return RejectSetup("want on|off", key, value);
    RadarAlt->SetPowered(value == "on");
    return true;
  }
  /* THE BRIEF is not applied here — the pilot enters it through the command bus once airborne, at its
   * control's latency class, and may be refused. Briefing nothing leaves every box as the spawn set it. */
  if (key == "brief_alow_ft" || key == "brief_bingo_lbs") {
    double v = 0.0;
    if (!ParseDouble(value, v)) return RejectSetup("not a number", key, value);
    if (key == "brief_alow_ft") PilotSys->BriefAlowFt(v);
    else PilotSys->BriefBingoLbs(v);
    return true;
  }
  if (key == "brief_master_arm") {
    if (value != "arm" && value != "sim") return RejectSetup("want arm|sim", key, value);
    PilotSys->BriefMasterArm(value == "arm");
    return true;
  }
  if (key == "brief_weapon") {
    if (value == "gun") PilotSys->BriefWeapon(1.0);
    else if (value == "aim9") PilotSys->BriefWeapon(2.0);
    else if (value == "aim120") PilotSys->BriefWeapon(3.0);
    else return RejectSetup("want gun|aim9|aim120", key, value);
    return true;
  }
  /* Forwarded WHOLE: the parameter set is a property of the PILOT, not of this airframe, so the key
   * table lives there and this module gets no second, drifting copy of it. */
  if (key.compare(0, 6, "pilot_") == 0) {
    double v = 0.0;
    if (!ParseDouble(value, v)) return RejectSetup("not a number", key, value);
    if (!PilotSys->ApplyTuning(key, v)) return RejectSetup("no such pilot parameter, or out of range",
                                                          key, value);
    return true;
  }
  if (key == "gear") {
    if (value != "up" && value != "down") return RejectSetup("want up|down", key, value);
    AirframeCtrl->SetGear(value == "down");
    return true;
  }
  /* One line per pylon, because that is how a jet is loaded. An unknown station or type is a mission
   * FAIL rather than a jet that quietly takes off with an empty rack. */
  if (key == "store") {
    std::istringstream in(value);
    int station = 0;
    std::string type;
    if (!(in >> station) || !(in >> type)) return RejectSetup("want '<station> <type>'", key, value);
    const FBStoreSpec *spec = FBFindStore(type.c_str());
    if (!spec) return RejectSetup("unknown store type", key, value);
    if (!SmsSys->Load(station, *spec)) return RejectSetup("no such station, or already loaded", key, value);
    return true;
  }
  /* Both briefed as a MOMENT and not a condition: mission-elapsed seconds, repeatable, travelling the
   * command bus like every other brief item and refusable there. */
  if (key == "brief_chaff_s") {
    double t = 0.0;
    if (!ParseDouble(value, t)) return RejectSetup("not a number", key, value);
    if (t < 0.0) return RejectSetup("negative time", key, value);
    if (!PilotSys->BriefChaff(t)) return RejectSetup("too many briefed dispenses", key, value);
    return true;
  }
  if (key == "brief_release_s") {
    double t = 0.0;
    if (!ParseDouble(value, t)) return RejectSetup("not a number", key, value);
    if (t < 0.0) return RejectSetup("negative time", key, value);
    if (!PilotSys->BriefRelease(t)) return RejectSetup("too many briefed releases", key, value);
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

} // namespace FlightBox::Modules
