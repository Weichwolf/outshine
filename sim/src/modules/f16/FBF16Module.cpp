#include "FBF16Module.h"
#include "FBF16Hud.h"
#include "FBLog.h"
#include "FBUnits.h"
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace FlightBox {

FBF16Module::FBF16Module()
    : AP(std::make_unique<FBAutopilot>()),
      FC(std::make_unique<FBFlightControl>(FBFlightControl::F16())),
      Input(std::make_unique<FBInputSystem>()),
      Propulsion(std::make_unique<FBPropulsionSystem>()),
      Disp(std::make_unique<FBF16Hud>()),   /* the F-16's own HUD symbology, not the generic default */
      Chip(std::make_unique<FBF16Max7456>()),
      Fcr_(std::make_unique<FBF16Fcr>()),   /* the F-16's own APG-68, not a generic search set */
      Weapons(std::make_unique<FBWeaponSystem>()),
      Rwr_(std::make_unique<FBF16Rwr>()),     /* the F-16's own ALR-56M, not a generic receiver */
      Cmds_(std::make_unique<FBF16Cmds>()),   /* ...and its own ALE-47 */
      Datalink_(std::make_unique<FBF16Datalink>()),
      AirData(std::make_unique<FBAirDataSystem>()),
      RadarAlt(std::make_unique<FBRadarAltimeter>()),
      NavSys(std::make_unique<FBNavSystem>()),
      FireCtrl(std::make_unique<FBF16FireControl>()),
      UfcSys(std::make_unique<FBF16Ufc>()),
      SmsSys(std::make_unique<FBF16Sms>()),
      Warn_(std::make_unique<FBWarningSystem>()),
      PilotSys(std::make_unique<FBF16Pilot>()),
      AirframeCtrl(std::make_unique<FBAirframeControls>()) {}   /* NoOp until an airframe is attached */

void FBF16Module::AttachFdm(FBFdm &fdm) {
  Fdm_ = &fdm;
  AirframeCtrl = std::make_unique<FBJsbsimAirframeControls>(fdm);
  /* The SMS's pylons become real point masses on THIS airframe (systems/FBStoresSystem::AttachFdm) —
   * every station empty until a mission loads one, so an unloaded jet is unchanged. */
  SmsSys->AttachFdm(fdm);
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
void FBF16Module::Run(fb_fdm_state &st, double dt, const FBUnitRegistry *units, const FBWorld *world) {
  if (!Fdm_) return;               /* no airframe attached (FBModule::AttachFdm) — nothing to fly */
  FBFdm &fdm = *Fdm_;
  SimTimeS += dt;                  /* the module's own clock — the datalink's message timestamps */
  SharedState.NowS = SimTimeS;     /* the bus time reference every block header is stamped against */
  /* What the DED gate reads: head-down data entry is only possible in a jet that is not being flown
   * hard (core/FBCommandBus.h). Unpublished air data reads as 1 g — a jet with no ADC is not
   * manoeuvring by any measurement this aircraft has. */
  CmdBus_.SetLoadFactor(SharedState.AirData.H.Readable() ? SharedState.AirData.GLoad : 1.0f);
  Input->Run(Mode, dt);            /* HOTAS/ICP: once per Run() call, the coarsest sim tick */
  Propulsion->Run(st, dt);         /* engine-system logic above the raw FDM: same cadence */

  if (Due(SensorAccS, dt, 10.0)) {
    /* The two blocks the module itself publishes, FIRST in the group because the systems below read
     * them: the platform pose out of the `st` this module was handed, and the airframe/propulsion
     * readbacks out of the controls object every gear/brake command already goes through. Publishing
     * them here rather than letting each consumer reach for `st` is what keeps "one writer per block"
     * true — FBF16FireControl used to read an altitude field nobody filled. */
    PublishPlatform(st);
    PublishAirframe();
    /* The two command groups whose owning boxes live in this slot group, serviced FIRST so a command
     * that becomes due now is already in force when those boxes run below — a switch the pilot threw
     * takes effect on the next sweep, not the one after it. */
    ServiceCommands(FBCommandGroup::Sensors);
    ServiceCommands(FBCommandGroup::Avionics);
    /* The pickle's group, serviced here with the others and immediately before the SMS's own tick
     * below, so a release is answered by the box that owns it and the stores block it republishes in
     * the same tick already reflects what left the jet. */
    ServiceCommands(FBCommandGroup::Stores);
    /* The Sensors slot: the SECOND (and last) system that sees the other units at all — the ACTIVE one,
     * next to the cooperative terminal below (systems/FBRadarSystem's banner). `units` stops here too;
     * what leaves is an anonymous contact list in SharedState. */
    Fcr_->Run(SharedState, st, units, SimTimeS);
    /* The HUD's telemetry chain, one throttle group so FireControl always reads Nav's SAME-tick output
     * (see the header's rate table) — `st` is the FDM state as of the END of the PREVIOUS Run() call,
     * same one-tick lag every other Sensor-cadence write already has. */
    AirData->Run(SharedState, st, dt);
    RadarAlt->Run(SharedState, (float)st.elev, GroundAslM);
    /* WHAT the nav system computes against: this module's own flight plan, republished every sensor
     * tick because the active waypoint advances during the run (NavSys->AdvanceWaypoint below). Only
     * the browser client ever set a steerpoint before, so in every .fbm run FBNavSystem had nothing to
     * publish and the whole readout chain that hangs off it (nav, cruise, fire-control) stayed Invalid.
     * The steerpoint's own GROUND elevation is not part of a `wp` line (doc/mission-format.md — a
     * waypoint declares the altitude to FLY, not the terrain under it), so the module supplies the only
     * terrain figure it has: this tick's elevation sample under the aircraft, the same one the radar
     * altimeter reads. Over the gentle terrain a route waypoint sits in that IS its elevation; a
     * declared per-waypoint elevation is mission-format work, not a number to invent here. */
    if (const FBWaypoint *swp = Plan_.ActiveWaypoint())
      NavSys->SetSteerpoint(swp->LatDeg, swp->LonDeg, GroundAslM * kMToFt);
    NavSys->Run(SharedState, st, dt);
    /* The fire control gets the SELECTED station's round: the launch zone it computes is for the weapon
     * that would actually leave the jet if the pilot pickled now (modules/f16/FBF16FireControl). */
    FireCtrl->Run(SharedState, st, FBStoreSpecOf(SmsSys->StoreAt(SmsSys->SelectedStation())), SimTimeS,
                  dt);
    /* ...and hands the SMS its target estimate, which the SMS copies onto a launched round and then
     * radiates as that round's midcourse uplink (systems/FBStoresSystem::SetTargetState). */
    SmsSys->SetTargetState(FireCtrl->TargetState());
    UfcSys->Run(SharedState, dt);
    SmsSys->Run(SharedState, dt);
    /* LAST in the group: the warning set is a pure consumer of everything published above it, including
     * their validity heads (systems/FBWarningSystem). */
    Warn_->Run(SharedState, dt);
  }
  if (Due(DisplayAccS, dt, 20.0)) Disp->Run(SharedState, Mode, dt);
  if (Due(WeaponAccS, dt, 20.0)) Weapons->Run(Mode, world, dt);
  /* The Defensive slot, at the pilot's own decision rate rather than the 5 Hz the NoOp placeholder ran
   * at: a countermeasure program's burst interval is a tenth of a second (doc/f16/defence-rwr-cm.md
   * §2.2), so entering the slot slower than the sim tick would quantise a salvo. The order inside it is
   * the data flow — the receiver writes the threat picture, the dispenser reads it, and the pilot's CMS
   * commands are answered in between so a throw takes effect on this tick's program and not the next. */
  if (Due(DefensiveAccS, dt, 10.0)) {
    Rwr_->Run(SharedState, st, units, SimTimeS);
    ServiceCommands(FBCommandGroup::Defensive);
    Cmds_->Run(SharedState, st, SimTimeS);
  }
  /* Comms/Datalink: the COOPERATIVE half of what this module knows about other units
   * (systems/FBDatalinkSystem's banner — `units`, the registry of published snapshots, reaches this slot
   * and the Sensors slot above, and nothing else). It writes tracks into SharedState; whatever reads
   * them — HUD today, pilot later — reads them as instrument data. */
  if (Due(CommsAccS, dt, 5.0)) {
    ServiceCommands(FBCommandGroup::Comms);
    Datalink_->Run(SharedState, st, units, SimTimeS);
  }
  /* Pilot: the mission brain above Guidance/FlightControl (rate table). Idle (nobody called SetPhase)
   * returns a neutral FBPilotCommands, so ApplyPilotCommands is a no-op until the App starts the phase
   * machine — once it does, this is the takeoff/climb/route chain actually flying the jet. Waypoint
   * capture (Akteurs-Verhalten, FBNavSystem's own job — doc/mission-format.md) runs right after, same
   * cadence: THIS tick's Pilot::Run() flew toward the pre-capture active waypoint. This module never
   * sees whether the mission itself concluded from that same capture — that verdict is a separate,
   * independent judgement the caller owns (core/, not this file). */
  if (Due(PilotAccS, dt, 10.0)) {
    ApplyPilotCommands(PilotSys->Run(SharedState, CmdBus_, *AirframeCtrl, st, Plan_,
                                     HaveRunway_ ? &Rwy_ : nullptr, dt));
    /* The pilot's own track processor publishes its fused picture onto the bus after the decision tick
     * — the same forwarding the platform block gets, for the same reason: the writer is one system
     * (systems/FBBfmTrack), it just does not hold the bus itself. */
    SharedState.Bfm = PilotSys->BfmTrack().Block();
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

void FBF16Module::PublishPlatform(const fb_fdm_state &st) {
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


/* One group's due commands, handed to the box that owns them. The MODULE does the routing for the same
 * reason it interprets its own `set` keys (FBModule::ApplySetup): the systems are generic, and which
 * F-16 box owns "radar mode" is this module's knowledge, not systems/'s. */
void FBF16Module::ServiceCommands(FBCommandGroup group) {
  FBAvionicsCommand c{};
  while (CmdBus_.TakeDue(group, SimTimeS, c)) {
    FBCommandOutcome outcome = FBCommandOutcome::Accepted;
    FBCommandReason reason = FBCommandReason::None;
    ApplyCommand(c, outcome, reason);
    CmdBus_.Complete(c, outcome, reason, SimTimeS);
  }
}

/* Where a command actually happens — and where the RANGE POLICY lives, next to the box that knows the
 * domain (core/FBAvionicsCommand.h's OutOfRange note): out of range is REJECTED and named, never
 * clamped behind the pilot's back. The one clamp in the file is the documented BNGO ceiling, and it is
 * reported as Clamped, not as success. */
void FBF16Module::ApplyCommand(const FBAvionicsCommand &c, FBCommandOutcome &outcome,
                               FBCommandReason &reason) {
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
      SmsSys->SetMasterArm(c.Value != 0.0 ? FBArmState::Arm : FBArmState::Sim);
      return;
    /* The two stores commands. The SMS itself answers the release (systems/FBStoresSystem::Release
     * decides and says why); this module only routes, exactly as it does for every other box. */
    case FBCommandTarget::StationSelect:
      if (!SmsSys->SelectStation((int)c.Value)) {
        outcome = FBCommandOutcome::Rejected;
        reason = FBCommandReason::OutOfContext;
      }
      return;
    case FBCommandTarget::WeaponRelease:
      SmsSys->Release(SimTimeS, outcome, reason);
      return;
    /* The defensive trio. The dispenser itself answers the throw (systems/FBCountermeasureSystem::
     * Dispense decides and says why — an empty magazine is a refusal with its own reason), exactly as
     * the SMS answers the pickle; this module only routes. */
    case FBCommandTarget::CmDispense:
      if (c.Value < 0.0 || c.Value > (double)FBCountermeasureSystem::kProgramCount) {
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
      /* Documented effect-side precondition (doc/f16/controls-commands.md §6.4): the entry commits
       * either way, but with the CARA unpowered the warning it arms can never fire — and the pilot is
       * told so instead of being left to assume a live floor. */
      if (!RadarAlt->Powered()) { outcome = FBCommandOutcome::Inhibited; reason = FBCommandReason::EffectPrecondition; }
      return;
    case FBCommandTarget::BingoLbs:
      if (!FBF16Ufc::BingoInRange((float)c.Value)) { outcome = FBCommandOutcome::Rejected; reason = FBCommandReason::OutOfRange; return; }
      /* §6.8: ENTR succeeds and the field shows what was typed; the warning still fires at the system
       * ceiling. Accepted-with-clamp, not a rejection. */
      if (!UfcSys->SetBingo((float)c.Value)) { outcome = FBCommandOutcome::Clamped; reason = FBCommandReason::ValueClamped; }
      return;
    case FBCommandTarget::SteerpointNum:
      if (!FBF16Ufc::SteerNumInRange((int)c.Value)) { outcome = FBCommandOutcome::Rejected; reason = FBCommandReason::OutOfRange; return; }
      UfcSys->SetSteerpointNumber((int)c.Value);
      return;
    /* §6.6, the honest answer: the F-16 has both, FlightBox has neither yet. A silent success would be
     * a lie the pilot would then fly on. */
    case FBCommandTarget::WeaponSelect:
    case FBCommandTarget::Designate:
      outcome = FBCommandOutcome::Rejected;
      reason = FBCommandReason::NotImplemented;
      return;
    case FBCommandTarget::None:
      outcome = FBCommandOutcome::Rejected;
      reason = FBCommandReason::OutOfContext;
      return;
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
  /* The MIDS terminal's switches (doc/f16/datalink-iff.md, systems/FBDatalinkSystem's banner): POWER and
   * XMT are two switches because the real terminal has two — powering it down blinds this jet, XMT OFF
   * only stops it being heard by the others. */
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
  /* The FCR + IFF set (doc/f16/radar-sensors.md, doc/f16/datalink-iff.md; modules/f16/FBF16Fcr's mode
   * table). `fcr_mode` is the ACM/CRM sub-mode selection the pilot would make on the HOTAS; `iff_xpdr`
   * and `iff_interrogator` are the two halves of the APX-113, separately switchable because they answer
   * two different questions — whether OTHERS can identify this jet, and whether this jet can identify
   * others. */
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
  /* The pilot's TASK: which phase of systems/FBPilot's machine this jet starts in. `route` is what a
   * mission gets without the line (FBMissionBoot's spawn already arms it); `bfm` puts the pilot into the
   * fight, where it flies its radar picture instead of a waypoint chain. Declared as mission data rather
   * than inferred from the loadout because two jets with identical setup lines can have opposite jobs —
   * one manoeuvres against the other, the other flies its brief. */
  if (key == "task") {
    if (value == "route") PilotSys->SetPhase(FBPilot::Phase::Route);
    else if (value == "bfm") PilotSys->SetPhase(FBPilot::Phase::Bfm);
    else return RejectSetup("want route|bfm", key, value);
    return true;
  }
  /* THE DEFENSIVE SUITE (doc/f16/defence-rwr-cm.md). `rwr` is the ALR-56M's POWER button; `rwr_display`
   * the TWP MODE button (PRIORITY = 5 symbols, OPEN = 16); `rwr_search` the TWA panel's SEARCH toggle,
   * which hides search-class emitters without hiding the fact that it is hiding them. `cmds_mode` is the
   * mode knob whose position decides who may dispense at all, `cmds_program` the PRGM knob, and
   * `cmds_chaff`/`cmds_flare` the loadout the ground crew set — capped at the airframe's documented 120
   * combined. */
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
  /* The CARA's power switch: the one mission-declarable way to make a source block INVALID, which is
   * what a consumer's handling of that state can be measured against (systems/FBRadarAltimeter). */
  if (key == "radalt") {
    if (value != "on" && value != "off") return RejectSetup("want on|off", key, value);
    RadarAlt->SetPowered(value == "on");
    return true;
  }
  /* THE BRIEF (systems/FBPilot's brief block): not applied here — handed to the pilot, who will enter
   * it through the avionics command bus once airborne, at its control's own latency class, and may be
   * refused. A mission that briefs nothing leaves every box exactly as the spawn set it. */
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
  if (key == "gear") {
    if (value != "up" && value != "down") return RejectSetup("want up|down", key, value);
    AirframeCtrl->SetGear(value == "down");
    return true;
  }
  /* THE LOADOUT, as mission data: `set store <station> <type>` (doc/mission-format.md). One line per
   * pylon, because that is how a jet is loaded — station by station, with the type on it. The station
   * numbers are this airframe's own (1..9, modules/f16/FBF16Sms), the type names are the catalogue's
   * (core/FBStore.h); an unknown either way is a mission FAIL rather than a jet that quietly takes off
   * with an empty rack. */
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
  /* WHEN the pilot pickles, as mission data: `set brief_release_s <t>`, repeatable — one line per store
   * the brief calls for, in mission-elapsed seconds. Deliberately a TIME and not a target: this stage
   * has no release solution (no CCIP/CCRP), so a mission that wants a bomb dropped from a defined state
   * says exactly that and nothing pretends to have aimed it. Like every other brief item it goes through
   * the command bus when the moment comes, and may be refused. */
  /* WHEN the pilot throws countermeasures, as mission data: `set brief_chaff_s <t>`, repeatable, one
   * line per dispense the brief calls for. The same shape and the same reasoning as brief_release_s —
   * a moment, not a condition — and it travels the same way, through the CMS switch's own HOTAS-class
   * command, which the dispenser may refuse (an empty magazine, the wrong knob position). The SEMI/AUTO
   * modes need no brief at all: there the jet answers its own RWR. */
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

} // namespace FlightBox
