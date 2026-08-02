#include "FBMig29Module.h"
#include "FBLog.h"
#include "FBUnits.h"
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <sstream>

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
      AirframeCtrl(std::make_unique<Systems::FBAirframeControls>()),
      FireCtrl_(std::make_unique<FBMig29FireControl>()) {
  /* Reihenfolge IST die Ordinalvergabe (ein Ordinal ist ein Kommandowert), erster Eintrag = Einschaltseite. */
  static const FBMfdPage kPages[] = {FBMfdPage::Fcr, FBMfdPage::Irst, FBMfdPage::Sms, FBMfdPage::Rwr,
                                     FBMfdPage::Sys};
  Mfd_.DeclarePages(kPages, (int)(sizeof kPages / sizeof kPages[0]));
}

void FBMig29Module::AttachFdm(Fdm::FBFdm &fdm) {
  Fdm_ = &fdm;
  AirframeCtrl = std::make_unique<Systems::FBJsbsimAirframeControls>(fdm);
  /* The pylons become real point masses on THIS airframe — every station empty until a mission loads
   * one, so an unloaded jet computes exactly as it did before stage 2c. */
  Stores_.AttachFdm(fdm);
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
  (void)world;   /* the TERRAIN side; no sensor here samples it (no masking is modelled) */
  if (!Fdm_) return;
  Fdm::FBFdm &fdm = *Fdm_;
  SimTimeS += dt;
  SharedState.NowS = SimTimeS;
  CmdBus_.SetLoadFactor(SharedState.AirData.H.Readable() ? SharedState.AirData.GLoad : 1.0f);

  if (Due(SensorAccS, dt, 10.0)) {
    PublishPlatform(st);
    PublishAirframe();
    /* Commands before the boxes they address, so a switch the pilot threw takes effect on the next
     * sweep and not the one after it. */
    ServiceCommands(FBCommandGroup::Sensors, st);
    ServiceCommands(FBCommandGroup::Avionics, st);
    /* THE TWO AIR-TO-AIR SENSORS, active first. `units` reaches exactly these two slots and the RWR
     * below, and nothing else in this module. The damage gate is the F-16's, minus the optical head:
     * core/FBSystemHealth has no id for an optical station, and adding one would move the dmg_*
     * telemetry columns — it belongs with the twin-engine change already listed in the module's gaps. */
    Radar_.SetRangeFactor(SystemDegraded(FBSystemId::Radar) ? kRadarRangeDegraded : 1.0);
    if (SystemWorking(FBSystemId::Radar)) Radar_.Run(SharedState, st, units, SimTimeS);
    else SharedState.Radar.H.Invalidate();
    Irst_.Run(SharedState, st, units, SimTimeS);
    Visual_.Run(SharedState, st, units, SimTimeS);   /* the eyes, ungated: not a box this jet carries */
    AirData->Run(SharedState, st, dt);
    RadarAlt->Run(SharedState, (float)st.elev, GroundAslM);
    if (const FBWaypoint *swp = Plan_.ActiveWaypoint())
      NavSys->SetSteerpoint(swp->LatDeg, swp->LonDeg, GroundAslM * kMToFt);
    NavSys->Run(SharedState, st, dt);
    /* The SELECTED station's round: the launch zone computed is for the weapon that would actually
     * leave the jet if the pilot pressed the trigger now. */
    if (SystemWorking(FBSystemId::FireControl))
      FireCtrl_->Run(SharedState, st, FBStoreSpecOf(Stores_.StoreAt(Stores_.SelectedStation())),
                     Gun_.Spec(), SimTimeS, dt);
    else SharedState.FireControl.H.Invalidate();
    /* What a launched round carries out of the jet — and, for a SEMI-ACTIVE round, what this jet then
     * has to keep illuminating for the whole time of flight. */
    Stores_.SetTargetState(FireCtrl_->TargetState());
    ServiceCommands(FBCommandGroup::Stores, st);
    /* THE AIRCRAFT LETS GO, and this is the one line that separates a director from a release cue:
     * the pilot's trigger was a CONSENT (serviced above), the release moment belongs to the computer.
     * The path out of the jet is still FBStoresSystem::Release and nothing else, so every interlock
     * (master arm, weight on wheels, the store's own envelope) answers exactly as it always did. */
    if (FireCtrl_->Director().ReleaseDue()) {
      FBCommandOutcome dout = FBCommandOutcome::Accepted;
      FBCommandReason dreason = FBCommandReason::None;
      Stores_.SetReleaseSolution(FireCtrl_->Director().Solution());
      bool ok = SystemWorking(FBSystemId::Stores) && Stores_.Release(SimTimeS, dout, dreason);
      FireCtrl_->Director().NotifyRelease(ok, SimTimeS);
    }
    if (SystemWorking(FBSystemId::Stores)) Stores_.Run(SharedState, dt);
    else SharedState.Stores.H.Invalidate();
    Warn_->Run(SharedState, dt);   /* LAST: a pure consumer of everything above it */
  }
  /* THE GUN, once per Run() with the FULL dt — deliberately unthrottled for the same reason the F-16's
   * is: its output is a round count INTEGRATED over time, so any other cadence would drop rounds or
   * invent them. */
  if (SystemWorking(FBSystemId::Gun)) Gun_.Run(SharedState, st, SimTimeS, dt);
  else SharedState.Gun.H.Invalidate();
  /* The PASSIVE half, and the one line that makes this jet's central defect real: what the world hears
   * from this aircraft is what deafens its own receiver forward. One bit, one source — the emission the
   * set derives from the pattern it is actually flying (FBRadarSystem::Emission), the same value the
   * tick barrier publishes to everybody else. */
  if (Due(DefensiveAccS, dt, 10.0)) {
    Rwr_.SetOwnRadiating(Radar_.Emission().Mode != FBEmitterMode::None);
    if (SystemWorking(FBSystemId::Rwr)) Rwr_.Run(SharedState, st, units, SimTimeS);
    else SharedState.Rwr.H.Invalidate();
    /* Data-flow order, the F-16's: the receiver wrote the threat picture above, due commands next so a
     * throw takes effect this sweep, then the dispenser reads the picture and answers. */
    ServiceCommands(FBCommandGroup::Defensive, st);
    if (SystemWorking(FBSystemId::Countermeasures)) Cm_.Run(SharedState, st, SimTimeS);
    else SharedState.Cmds.H.Invalidate();
  }
  /* Die Bank vor der Anzeigenlogik, aus demselben Grund wie in der F-16: erst schneiden und
   * veroeffentlichen, dann zeichnet, was den Block liest. */
  if (Due(DisplayAccS, dt, 20.0)) { Mfd_.Run(SharedState, SimTimeS); Disp->Run(SharedState, Mode, dt); }

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


/* THE COMMAND ROUTER. Same shape as the F-16's and for the same reason: FBCommandTarget names a
 * FUNCTION ("radar mode"), and which box on THIS panel answers it is the aircraft's knowledge. Two
 * targets mean something different here than they do over there, and both differences are documented
 * controls rather than reinterpretations:
 *   RadarSlewAz -> the ZONE switch. This antenna is not slewed continuously in azimuth; the 130° field
 *                  is divided into three overlapping sectors and the pilot picks one. A continuous
 *                  value is therefore SNAPPED to the nearest sector, and that is a Clamped outcome —
 *                  reported, not silently rounded.
 *   RadarSlewEl -> the antenna elevation knob, which on this jet is the OUTPUT of the range-angle
 *                  entry the GCI loop runs through (doc/modules/mig29/datalink-gci.md §2.2). */
void FBMig29Module::ServiceCommands(FBCommandGroup group, const Fdm::fb_fdm_state &st) {
  FBAvionicsCommand c{};
  while (CmdBus_.TakeDue(group, SimTimeS, c)) {
    FBCommandOutcome outcome = FBCommandOutcome::Accepted;
    FBCommandReason reason = FBCommandReason::None;
    ApplyCommand(c, st, outcome, reason);
    CmdBus_.Complete(c, outcome, reason, SimTimeS);
  }
}

void FBMig29Module::ApplyCommand(const FBAvionicsCommand &c, const Fdm::fb_fdm_state &st,
                                 FBCommandOutcome &outcome, FBCommandReason &reason) {
  auto reject = [&](FBCommandReason r) { outcome = FBCommandOutcome::Rejected; reason = r; };
  switch (c.Target) {
    case FBCommandTarget::RadarMode: {
      int ord = (int)c.Value;
      if (ord < 0 || ord > (int)FBMig29RadarMode::Acm) { reject(FBCommandReason::OutOfRange); return; }
      if (!SystemWorking(FBSystemId::Radar)) { reject(FBCommandReason::SystemFailed); return; }
      Radar_.SetMode((FBMig29RadarMode)ord);
      return;
    }
    case FBCommandTarget::RadarEmission: {
      int ord = (int)c.Value;
      if (ord < 0 || ord > (int)FBMig29Emission::Off) { reject(FBCommandReason::OutOfRange); return; }
      if (!SystemWorking(FBSystemId::Radar)) { reject(FBCommandReason::SystemFailed); return; }
      Radar_.SetEmission((FBMig29Emission)ord);
      return;
    }
    case FBCommandTarget::RadarRangeNm:
      if (c.Value < 0.0) { reject(FBCommandReason::OutOfRange); return; }
      Radar_.SetRangeOverrideNm(c.Value);
      return;
    case FBCommandTarget::RadarSlewAz: {
      /* Snap to the nearest of the three sectors. A pilot turning a three-position switch cannot land
       * between two of them, so the command SUCCEEDS with the position it actually reached. */
      double v = c.Value;
      FBMig29Zone z = v < -FBMig29Radar::kZoneOffsetDeg * 0.5 ? FBMig29Zone::Left
                    : v > FBMig29Radar::kZoneOffsetDeg * 0.5 ? FBMig29Zone::Right : FBMig29Zone::Center;
      Radar_.SetZone(z);
      double reached = z == FBMig29Zone::Left ? -FBMig29Radar::kZoneOffsetDeg
                     : z == FBMig29Zone::Right ? FBMig29Radar::kZoneOffsetDeg : 0.0;
      if (reached != v) { outcome = FBCommandOutcome::Clamped; reason = FBCommandReason::ValueClamped; }
      return;
    }
    case FBCommandTarget::RadarSlewEl: {
      double before = Radar_.AntennaElevDeg();
      Radar_.SetAntennaElevDeg(c.Value);
      if (Radar_.AntennaElevDeg() != c.Value && before != c.Value) {
        outcome = FBCommandOutcome::Clamped;   /* the dish reached its own stop */
        reason = FBCommandReason::ValueClamped;
      }
      return;
    }
    case FBCommandTarget::IffTransponder: Radar_.SetIffTransponder(c.Value != 0.0); return;
    case FBCommandTarget::IffInterrogator: Radar_.SetIffInterrogator(c.Value != 0.0); return;
    case FBCommandTarget::Designate:
      /* ONE BUTTON, TWO JOBS — which one it does is decided by the A/A-A/G switch ([DCS-EA p.59]),
       * and the only form this tree can state that switch in is the round under the trigger: with an
       * unguided store selected the LOCKON press is the ground procedure's preliminary target
       * acquisition and it fires the rangefinder, not the radar. */
      if (FireCtrl_->Director().Engaged()) {
        if (!SystemWorking(FBSystemId::FireControl)) { reject(FBCommandReason::SystemFailed); return; }
        if (!FireCtrl_->Director().Range(SharedState, st, SimTimeS))
          reject(FBCommandReason::OutOfContext);
        return;
      }
      if (!SystemWorking(FBSystemId::Radar)) { reject(FBCommandReason::SystemFailed); return; }
      if (!Radar_.Designate((int)c.Value, SimTimeS)) {
        /* The return was gone by the time the hand had finished — the same answer the F-16 gives. */
        reject(FBCommandReason::OutOfContext);
      }
      return;
    case FBCommandTarget::IrstMode: {
      int ord = (int)c.Value;
      if (ord < 0 || ord > (int)FBMig29IrstMode::Bore) { reject(FBCommandReason::OutOfRange); return; }
      Irst_.SetMode((FBMig29IrstMode)ord);
      Irst_.SetPowered(ord != (int)FBMig29IrstMode::Off);
      return;
    }
    case FBCommandTarget::IrstDesignate:
      if (!Irst_.Designate((int)c.Value, SimTimeS)) { reject(FBCommandReason::OutOfContext); return; }
      return;
    case FBCommandTarget::IrstLaser:
      /* The laser is collimated with the head: without a tracked source there is nothing to range, and
       * arming it would report a capability the station does not have at that moment. */
      if (c.Value != 0.0 && !Irst_.Locked()) { reject(FBCommandReason::OutOfContext); return; }
      Irst_.SetLaserArmed(c.Value != 0.0);
      return;
    /* ---- STAGE 2c: the weapons. PSR-31's MASTER ARM is one switch for both weapon systems on this
     * panel exactly as it is on the F-16's ([DCS-EA p.12] wires the stick's combat triggers, plural). */
    case FBCommandTarget::MasterArm:
      if (!SystemWorking(FBSystemId::Stores)) { reject(FBCommandReason::SystemFailed); return; }
      Stores_.SetMasterArm(c.Value != 0.0 ? FBArmState::Arm : FBArmState::Sim);
      Gun_.SetMasterArm(Stores_.MasterArm());
      return;
    case FBCommandTarget::StationSelect:
      if (!SystemWorking(FBSystemId::Stores)) { reject(FBCommandReason::SystemFailed); return; }
      /* The external-stores selector of [DCS-EA p.60] picks a PAIR, not a station; FlightBox releases
       * one store per command (doc/modules/mig29/weapons.md §2.4's SINGLE case), so the value is a
       * station number and the pair semantics are the named gap. */
      if (!Stores_.SelectStation((int)c.Value)) reject(FBCommandReason::OutOfContext);
      return;
    case FBCommandTarget::WeaponRelease:
      if (!SystemWorking(FBSystemId::Stores)) { reject(FBCommandReason::SystemFailed); return; }
      /* THE TRIGGER IS NOT THE SAME ACTION FOR BOTH HALVES OF THIS AIRCRAFT. Against an air target it
       * launches: press = away. Against the ground it CONSENTS, and the aircraft releases when its own
       * countdown runs out ([DCS-EA p.101], doc/modules/mig29/weapons.md §5.4). Value 0 = the pilot let
       * go, which the source explicitly permits and which abandons the countdown. */
      if (FireCtrl_->Director().Engaged()) {
        if (!SystemWorking(FBSystemId::FireControl)) { reject(FBCommandReason::SystemFailed); return; }
        if (!FireCtrl_->Director().Consent(c.Value != 0.0, SharedState, st, SimTimeS))
          reject(FBCommandReason::OutOfContext);
        return;
      }
      Stores_.Release(SimTimeS, outcome, reason);   /* the BOX decides and says why; this only routes */
      return;
    case FBCommandTarget::GunTrigger:
      if (!SystemWorking(FBSystemId::Gun)) { reject(FBCommandReason::SystemFailed); return; }
      Gun_.Trigger(c.Value, SimTimeS, outcome, reason);
      return;
    /* ---- The BVP-30-26 dispensers. Same three CMS controls as the F-16; the box answers itself
     * (empty magazine), the module only routes and gates on the failed dispenser. */
    case FBCommandTarget::CmDispense:
      if (!SystemWorking(FBSystemId::Countermeasures)) { reject(FBCommandReason::SystemFailed); return; }
      if (c.Value < 0.0 || c.Value > (double)Sensors::FBCountermeasureSystem::kProgramCount) {
        reject(FBCommandReason::OutOfRange);
        return;
      }
      Cm_.Dispense((int)c.Value, SimTimeS, outcome, reason);
      return;
    case FBCommandTarget::CmConsent:
      Cm_.SetConsent(c.Value != 0.0);
      return;
    case FBCommandTarget::CmdsMode: {
      int ord = (int)c.Value;
      if (ord < 0 || ord > (int)FBCmdsMode::Byp) { reject(FBCommandReason::OutOfRange); return; }
      Cm_.SetMode((FBCmdsMode)ord);
      return;
    }
    /* Wert = das Seitenordinal DIESES Cockpits; was der Katalog gerade nicht hergibt, ist
     * OutOfContext — der Knopf ist da, die Seite nicht. */
    case FBCommandTarget::MfdPageSelect:
      if (!Mfd_.Select((int)c.Value, SimTimeS)) reject(FBCommandReason::OutOfContext);
      return;
    default:
      /* Every remaining target names a box this aircraft does not compose (datalink, dispensers, UFC).
       * Answering it would report success for a switch with nothing behind it. */
      reject(FBCommandReason::NotImplemented);
      return;
  }
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

/* Four whitespace-separated numbers and nothing else. Strict for the same reason ParseDouble is: a
 * half-parsed controller call would put the antenna somewhere nobody briefed. */
bool ParseGciCall(const std::string &v, double &atS, double &brgDeg, double &rangeKm, double &altKm) {
  double out[4] = {0.0, 0.0, 0.0, 0.0};
  const char *p = v.c_str();
  for (int i = 0; i < 4; i++) {
    char *end = nullptr;
    errno = 0;
    out[i] = std::strtod(p, &end);
    if (end == p || errno == ERANGE || !std::isfinite(out[i])) return false;
    p = end;
  }
  while (*p == ' ' || *p == '\t') p++;
  if (*p) return false;
  atS = out[0]; brgDeg = out[1]; rangeKm = out[2]; altKm = out[3];
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
  if (ApplyJammerSetup(key, value)) return true;
  if (key == "gear") {
    if (value != "up" && value != "down") return RejectSetup("want up|down", key, value);
    AirframeCtrl->SetGear(value == "down");
    return true;
  }
  if (key == "task") {
    /* `intercept` came with stage 2b (the BVR phase machine needs a radar), `bfm` with 2c (it needs a
     * weapon). `attack` is the LAST one and it needed neither: it needed a DELIVERY PROCEDURE, because
     * this aircraft has no release cue to pickle on and never will. What unlocks it is
     * FBMig29Director — the same trigger, a different meaning (doc/modules/mig29/weapons.md §5.4). */
    if (value == "route") { PilotSys->SetPhase(Pilot::FBPilot::Phase::Route); return true; }
    if (value == "intercept") { PilotSys->SetPhase(Pilot::FBPilot::Phase::Intercept); return true; }
    if (value == "bfm") { PilotSys->SetPhase(Pilot::FBPilot::Phase::Bfm); return true; }
    if (value == "attack") { PilotSys->SetPhase(Pilot::FBPilot::Phase::Attack); return true; }
    /* `formation` needs no datalink to be DECLARED — but this jet has none, so its station keeping has
     * no lead report to hold and falls straight through to its own route. That is the doctrine, not a
     * gap: a MiG-29 flight is held together by the controller, not by a net. */
    if (value == "formation") { PilotSys->SetPhase(Pilot::FBPilot::Phase::Formation); return true; }
    return RejectSetup("want route|intercept|bfm|attack|formation", key, value);
  }
  /* THE ONLY VALUE THIS AIRFRAME ACCEPTS, and the refusal of the other three is the statement: `ccip`
   * and `ccrp` name a release cue this computer does not publish, `arm` names a weapon it does not
   * carry. `opt` is not a fourth instrument — it is the aircraft's own procedure, in which the pilot
   * consents and the aircraft releases. doc/modules/mig29/weapons.md §5.4. */
  if (key == "attack_mode") {
    if (value != "opt")
      return RejectSetup("want opt (this jet has no CCIP/CCRP computer and no anti-radiation store)",
                         key, value);
    PilotSys->BriefAttack(FBDeliveryMode::Opt);
    return true;
  }
  /* THE SORT CONTRACT. On this aircraft it is the ONLY sort there is: no cooperative terminal, so the
   * controller's split is agreed on the ground and applied by each pilot to the picture he has.
   * doc/formation.md, section 5.3. */
  if (key == "brief_sort") {
    Pilot::FBSortContract sc;
    if (!Pilot::FBSortContractFromString(value.c_str(), sc))
      return RejectSetup("want none|left|right|near|far", key, value);
    PilotSys->BriefSort(sc);
    return true;
  }
  /* ---- STAGE 2c: the weapons. Same key spellings the F-16 answers, because they name GENERIC
   * properties (a pylon, a drum, a switch) rather than this aircraft's boxes. */
  if (key == "store") {
    std::istringstream in(value);
    int station = 0;
    std::string type;
    if (!(in >> station) || !(in >> type)) return RejectSetup("want '<station> <type>'", key, value);
    const FBStoreSpec *spec = FBFindStore(type.c_str());
    if (!spec) return RejectSetup("unknown store type", key, value);
    if (!Stores_.Load(station, *spec)) return RejectSetup("no such station, or already loaded", key, value);
    return true;
  }
  if (key == "gun_rounds") {
    double n = 0.0;
    if (!ParseDouble(value, n)) return RejectSetup("not a number", key, value);
    if (n < 0.0 || n != std::floor(n)) return RejectSetup("want a whole, non-negative round count", key, value);
    if (!Gun_.SetRounds((int)n)) return RejectSetup("more rounds than the gun holds", key, value);
    return true;
  }
  /* Briefed as a MOMENT and not a condition: mission-elapsed seconds, repeatable, travelling the
   * command bus like every other brief item and refusable there. */
  if (key == "brief_release_s") {
    double t = 0.0;
    if (!ParseDouble(value, t)) return RejectSetup("not a number", key, value);
    if (t < 0.0) return RejectSetup("negative time", key, value);
    if (!PilotSys->BriefRelease(t)) return RejectSetup("too many briefed releases", key, value);
    return true;
  }
  /* Ein MOMENT und eine DAUER: "<sekunden> <burstS>". Dieselbe Form wie brief_release_s und aus
   * demselben Grund — ein Feuerstoss ist eine Handlung, kein Zustand. */
  if (key == "brief_gun_s") {
    std::istringstream in(value);
    double atS = 0.0, burstS = 0.0;
    if (!(in >> atS) || !(in >> burstS)) return RejectSetup("want '<atS> <burstS>'", key, value);
    if (atS < 0.0 || burstS <= 0.0) return RejectSetup("want a time >= 0 and a burst > 0", key, value);
    if (!PilotSys->BriefGun(atS, burstS)) return RejectSetup("too many briefed bursts", key, value);
    return true;
  }
  if (key == "brief_master_arm") {
    if (value != "arm" && value != "sim") return RejectSetup("want arm|sim", key, value);
    PilotSys->BriefMasterArm(value == "arm");
    return true;
  }
  /* A briefed dispense, one line per moment — the same channel the F-16 uses (BriefChaff throws the
   * SELECTED program, so a FLARE program here answers an infrared shot). The generic key name is kept:
   * the pilot's brief holds "throw the current program at time t" regardless of what is in it. */
  if (key == "brief_chaff_s" || key == "brief_flare_s") {
    double t = 0.0;
    if (!ParseDouble(value, t)) return RejectSetup("not a number", key, value);
    if (t < 0.0) return RejectSetup("negative time", key, value);
    if (!PilotSys->BriefChaff(t)) return RejectSetup("too many briefed dispenses", key, value);
    return true;
  }
  if (key == "brief_weapon") {
    /* This aircraft's own inventory, not the F-16's: the numbers are the pilot's brief ordinals. */
    if (value == "gun") PilotSys->BriefWeapon(1.0);
    else if (value == "r73") PilotSys->BriefWeapon(2.0);
    else if (value == "r27r") PilotSys->BriefWeapon(3.0);
    else return RejectSetup("want gun|r73|r27r", key, value);
    return true;
  }
  /* Forwarded WHOLE, exactly as the F-16 forwards it: the parameter set is a property of the PILOT. */
  if (key.compare(0, 6, "pilot_") == 0) {
    double v = 0.0;
    if (!ParseDouble(value, v)) return RejectSetup("not a number", key, value);
    if (!PilotSys->ApplyTuning(key, v)) return RejectSetup("no such pilot parameter, or out of range",
                                                          key, value);
    return true;
  }
  /* ---- The N019, its own key prefix. A key that names a GENERIC system property (iff_*) is shared
   * with the F-16; a key that names THIS aircraft's box is not, because `fcr_mode crm` means nothing on
   * a jet whose modes are RAD/CC/VS/BORE. */
  if (key == "n019_mode") {
    FBMig29RadarMode m = FBMig29RadarMode::Off;
    if (!FBMig29RadarModeFromString(value.c_str(), m))
      return RejectSetup("want off|rad|cc|vs|bore", key, value);
    Radar_.SetMode(m);
    return true;
  }
  if (key == "n019_emission") {
    if (value == "illum") Radar_.SetEmission(FBMig29Emission::Illum);
    else if (value == "dummy") Radar_.SetEmission(FBMig29Emission::Dummy);
    else if (value == "off") Radar_.SetEmission(FBMig29Emission::Off);
    else return RejectSetup("want illum|dummy|off", key, value);
    return true;
  }
  if (key == "n019_zone") {
    if (value == "left") Radar_.SetZone(FBMig29Zone::Left);
    else if (value == "center") Radar_.SetZone(FBMig29Zone::Center);
    else if (value == "right") Radar_.SetZone(FBMig29Zone::Right);
    else return RejectSetup("want left|center|right", key, value);
    return true;
  }
  if (key == "n019_elev") {
    double deg = 0.0;
    if (!ParseDouble(value, deg)) return RejectSetup("not a number", key, value);
    Radar_.SetAntennaElevDeg(deg);
    return true;
  }
  if (key == "n019_range_nm") {
    double nm = 0.0;
    if (!ParseDouble(value, nm)) return RejectSetup("not a number", key, value);
    if (nm < 0.0) return RejectSetup("negative range", key, value);
    Radar_.SetRangeOverrideNm(nm);
    return true;
  }
  if (key == "iff_xpdr" || key == "iff_interrogator") {
    if (value != "on" && value != "off") return RejectSetup("want on|off", key, value);
    if (key == "iff_xpdr") Radar_.SetIffTransponder(value == "on");
    else Radar_.SetIffInterrogator(value == "on");
    return true;
  }
  /* ---- The SPO-15. `rwr` and `rwr_search` are the same generic properties the F-16 answers (power
   * and the search filter), so they keep the same names. */
  if (key == "rwr" || key == "rwr_search") {
    if (value != "on" && value != "off") return RejectSetup("want on|off", key, value);
    if (key == "rwr") Rwr_.SetPowered(value == "on");
    else Rwr_.SetSearchShown(value == "on");
    return true;
  }
  /* ---- The BVP-30-26 dispensers. Same generic keys the F-16 answers — the mode knob, the PRGM knob
   * and the ground-crew loadout — bounded by this jet's smaller 60-cartridge magazine. */
  if (key == "cmds_mode") {
    FBCmdsMode m;
    if (!FBCmdsModeFromString(value.c_str(), m))
      return RejectSetup("want off|stby|man|semi|auto|byp", key, value);
    Cm_.SetMode(m);
    return true;
  }
  if (key == "cmds_program") {
    double n = 0.0;
    if (!ParseDouble(value, n)) return RejectSetup("not a number", key, value);
    if (!Cm_.SelectProgram((int)n)) return RejectSetup("no such program (1..6)", key, value);
    return true;
  }
  if (key == "cmds_chaff" || key == "cmds_flare") {
    double n = 0.0;
    if (!ParseDouble(value, n)) return RejectSetup("not a number", key, value);
    if (n < 0.0 || n > (double)FBMig29Cmds::kMaxCombined)
      return RejectSetup("outside 0..60", key, value);
    int chaff = key == "cmds_chaff" ? (int)n : Cm_.ChaffRemaining();
    int flare = key == "cmds_flare" ? (int)n : Cm_.FlareRemaining();
    if (chaff + flare > FBMig29Cmds::kMaxCombined)
      return RejectSetup("chaff + flare exceeds the 60-cartridge magazine", key, value);
    Cm_.SetLoadout(chaff, flare);
    return true;
  }
  /* ---- The EYES: the same generic three keys the F-16 answers, unprefixed because they name a
   * property of the pilot rather than a box of this aircraft (doc/missions/sensors.md). */
  if (key == "visual") {
    if (value != "on" && value != "off") return RejectSetup("want on|off", key, value);
    Visual_.SetPowered(value == "on");
    return true;
  }
  if (key == "visual_cone_az" || key == "visual_cone_el") {
    char *end = nullptr;
    double v = std::strtod(value.c_str(), &end);
    if (end == value.c_str() || *end != '\0' || v <= 0.0 || v > 180.0)
      return RejectSetup("want half-angle 0..180 deg", key, value);
    if (key == "visual_cone_az") Visual_.SetCone(v, Visual_.Field().ElHalfDeg);
    else Visual_.SetCone(Visual_.Field().AzHalfDeg, v);
    return true;
  }
  /* ---- The KOLS. */
  if (key == "kols_mode") {
    FBMig29IrstMode m = FBMig29IrstMode::Off;
    if (!FBMig29IrstModeFromString(value.c_str(), m))
      return RejectSetup("want off|ir|ir_cc|bore", key, value);
    Irst_.SetMode(m);
    Irst_.SetPowered(m != FBMig29IrstMode::Off);
    return true;
  }
  if (key == "kols_laser") {
    if (value != "on" && value != "off") return RejectSetup("want on|off", key, value);
    Irst_.SetLaserArmed(value == "on");
    return true;
  }
  /* ---- GCI: the controller's call as MISSION DATA. One line per transmission, four numbers:
   *   set brief_gci <atS> <bearingDeg> <rangeKm> <altKm>
   * i.e. the BRAA the manuals put in the controller's mouth (bearing, range in KILOMETRES because the
   * controller is Russian, and the target's ABSOLUTE altitude). It is not knowledge: it is something
   * the pilot has to TYPE, one entry per decision tick, in the DED latency class, and he can be turned
   * away by the bus like anybody else. doc/missions/sensors.md. */
  if (key == "brief_gci") {
    double atS = 0.0, brgDeg = 0.0, rangeKm = 0.0, altKm = 0.0;
    if (!ParseGciCall(value, atS, brgDeg, rangeKm, altKm))
      return RejectSetup("want '<atS> <bearingDeg> <rangeKm> <altKm>'", key, value);
    if (atS < 0.0 || rangeKm <= 0.0) return RejectSetup("time < 0 or range <= 0", key, value);
    if (!PilotSys->BriefGci(atS, brgDeg, rangeKm, altKm))
      return RejectSetup("more GCI calls than the brief holds", key, value);
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
