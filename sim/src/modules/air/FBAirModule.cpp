#include "FBAirModule.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <sstream>

#include "FBGeodesy.h"
#include "FBLog.h"
#include "FBUnits.h"

namespace FlightBox::Modules {

namespace {

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

/* The row's own telemetry, appended after everything units/FBSimUnit registers by name — the one
 * conditional source hook a module has, and the reason a mission that declares no catalogue row writes
 * a byte-identical file. */
class FBAirTelemetry : public FBTelemetrySource {
public:
  explicit FBAirTelemetry(const FBAirModule &m) : M_(m) {}
  const char *TelemetryName() const override { return "air"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override {
    schema.Add("air_tier");         /* FBAirTier ordinal */
    schema.Add("air_state");        /* the pilot phase ordinal, Orbit/Drag included */
    schema.Add("air_cue_age_s", "s");   /* the two staleness terms summed; -1 = uncued */
    schema.Add("air_cues");         /* how many antenna pointings this aircraft has been given */
    schema.Add("air_drag_s", "s");
    /* ---- THE FIRE CONTROL'S LIVE ANSWER. A13 was found because there was no column to find it in:
     * eng_* carries the shot's own snapshot and therefore says nothing at all about an aircraft that
     * never shot. These nine say, tick by tick, whether this row COULD have. A row with no fire control
     * (every mover, every T0/T1) reports the same all-zero line it reported before they existed. ---- */
    schema.Add("fc_dlz");             /* 1 = a launch zone exists for the round on the selected rail */
    schema.Add("fc_in_zone");         /* 1 = Rmin <= range <= Raero, what the release interlock reads */
    schema.Add("fc_rng_nm", "nm");
    schema.Add("fc_rtr_nm", "nm");
    schema.Add("fc_raero_nm", "nm");
    schema.Add("fc_tta_s", "s");      /* -1 = this round NEVER lets go: the shooter is bound to impact */
    schema.Add("fc_gun_err_deg", "deg");
    schema.Add("fc_gun_funnel");      /* 1 = in range AND inside the lead solution's own tolerance */
    schema.Add("fc_bound");           /* 1 = a round in flight still owes this aircraft its illumination */
  }
  void SampleTelemetry(FBTelemetryRow &row) const override {
    row.Push((int)M_.Spec_.Tier);
    row.Push((int)M_.Pilot_.GetPhase());
    row.Push(M_.CueAgeS_);
    row.Push(M_.CueCount_);
    row.Push(M_.Pilot_.DragElapsedS());
    const FBFireControlBlock &fc = M_.State_.FireControl;
    bool ok = fc.H.Readable();
    row.Push(ok && fc.DlzValid ? 1 : 0);
    row.Push(ok && fc.InZone ? 1 : 0);
    row.Push(ok && fc.DlzValid ? fc.TargetRangeM * kMToNm : -1.0);
    row.Push(ok && fc.DlzValid ? fc.RtrM * kMToNm : -1.0);
    row.Push(ok && fc.DlzValid ? fc.RaeroM * kMToNm : -1.0);
    row.Push(ok && fc.DlzValid ? (double)fc.TimeToActiveS : 0.0);
    row.Push(ok && fc.GunValid ? (double)fc.GunAimErrorDeg : -1.0);
    row.Push(ok && fc.GunInFunnel ? 1 : 0);
    row.Push(M_.Pilot_.Bound() ? 1 : 0);
  }

private:
  const FBAirModule &M_;
};

FBAirModule::FBAirModule(const FBAircraftSpec &spec)
    : Spec_(spec),
      /* THE RAW-AIRFRAME PATH with THIS ROW'S OWN numbers (recipe §6.1). Every catalogue deck is a raw
         airframe, so the MiG-29's gain set was the right FORM — but its 26 deg alpha limiter on a
         MiG-25 (12 deg) parks the limiter 14 deg past the stall, and its pitch authority on a row with
         three times the elevator power is three times the loop gain. Measured before:
         air-bomber-intercept.fbm flew 8 000 m into the ground in 242 s holding its commanded speed. */
      FC_(Systems::FBFlightControl::Raw(spec.Perf.PitchStickMax, spec.Perf.AlphaLimitDeg,
                                        spec.Perf.MaxG)),
      Ctrl_(std::make_unique<Systems::FBAirframeControls>()),
      Tele_(std::make_unique<FBAirTelemetry>(*this)) {
  Radar_.Configure(spec.Radar);
  Radar_.SetPowered(spec.Radar.SearchRangeM > 0.0);
  /* AN EARLY-WARNING NODE INTERROGATES, and it is the ONE identity channel it is allowed: Mode 4 is
   * two-valued and knows no "hostile", so what it buys the node is the ability to leave its OWN side
   * out of a report — nothing more. Without it the node reports its nearest echo whatever that is, and
   * measured, that is a cue onto the friendly fighter it was supposed to be cueing. */
  Radar_.SetIffInterrogator(spec.NetNode);
  Radar_.SetIffTransponder(true);
  /* A row with no receiver CANNOT defend, cannot be cued by an emission and cannot run the drag reflex
   * — a MiG-17 has none, and the absence is the model. */
  Rwr_.SetPowered(spec.HasRwr);
  Irst_.SetPowered(spec.IrstRangeM > 0.0);
  /* THE EYE, verbatim and for every row: its per-row input is the presented extent, i.e. span and
   * length, which is the ONE sensor input this catalogue can source for every row without exception. */
  Eye_.SetPowered(true);
  /* The cooperative terminal is a T4 property — an eastern interceptor is held together by a
   * controller, not by a net (module.md §Spec 5, tier T3 has no cooperative half by doctrine). */
  /* A T4 row carries the cooperative terminal (Link-16 PPLI); an early-warning NODE carries one too,
   * because its report has to reach somebody — and it is the SAME box, which is exactly why a member's
   * controller feed needs a block of its own rather than this one. */
  Datalink_.SetPowered(spec.Tier == FBAirTier::Peer || spec.NetNode);
  NetLink_.SetPowered(spec.NetMember);
  NetLink_.SetTransmit(false);   /* a member LISTENS to the controller; it does not report back */

  /* THE FIRE CONTROL IS A DECLARED WEAPON'S COMPANION, and a row without one gets no computer at all —
   * see FBAircraftSpec::CanEmploy. Until this existed, FBState::FireControl was never written for ANY
   * row and all three of pilot/FBPilot's employment gates stayed shut on all eighteen (module.md A13). */
  if (spec.CanEmploy()) {
    Fc_ = std::make_unique<FBAirFireControl>();
    Fc_->Configure(spec);
  }

  Pilot_.Configure(spec);
  /* T0 holds a station and T1 holds a station with one reflex — both start in Orbit, and every tier
   * above them starts Idle until the mission declares a task. */
  if (spec.Tier == FBAirTier::Track || spec.Tier == FBAirTier::Reflex)
    Pilot_.SetPhase(Pilot::FBPilot::Phase::Orbit);
  Mover_.Configure(spec.Mover);

  for (int i = 1; i <= spec.Stations; i++) {
    /* Pylon geometry from the row's own span [DERIVED]: stations alternate outboard in pairs, at the
     * quarter- and half-semispan a conventional fighter's wing pylons sit at. Nothing publishes the
     * real stations for any row, and the ONE thing this geometry reaches is the store's separation
     * point — a metre of it changes no outcome the campaigns measure. */
    double halfSpanM = spec.Layout.FrontalExtentM;
    double side = (i % 2 == 1) ? -1.0 : 1.0;
    double frac = 0.25 + 0.20 * ((i - 1) / 2);
    Stores_.DeclareStation(i, 0.0, side * halfSpanM * frac * 39.37, -20.0);
  }
  if (spec.Gun != FBGunKind::None) {
    if (const FBGunSpec *g = FBGunSpecOf(spec.Gun)) {
      Gun_.Install(*g, 0.0, 0.0, 0.0, /*boreDownDeg*/ 0.0, /*boreRightDeg*/ 0.0);
      (void)Gun_.SetRounds(spec.GunRounds <= g->Capacity ? spec.GunRounds : g->Capacity);
    }
  }
}

FBAirModule::~FBAirModule() = default;

void FBAirModule::AttachFdm(Fdm::FBFdm &fdm) {
  Fdm_ = &fdm;
  Ctrl_ = std::make_unique<Systems::FBJsbsimAirframeControls>(fdm);
  Stores_.AttachFdm(fdm);
}

FBTelemetrySource *FBAirModule::NetTelemetry() { return Tele_.get(); }

bool FBAirModule::Due(double &accS, double dt, double hz) {
  accS += dt;
  double period = 1.0 / hz;
  if (accS < period) return false;
  accS -= period;
  return true;
}

void FBAirModule::PublishPlatform(const Fdm::fb_fdm_state &st) {
  FBPlatformBlock &b = State_.Platform;
  b.RollDeg = (float)st.roll;
  b.PitchDeg = (float)st.pitch;
  b.YawDeg = (float)st.yaw;
  b.AltM = (float)st.elev;
  b.GsMs = (float)st.gs;
  b.TasMs = (float)st.speed;
  b.VsMs = (float)st.vy;
  b.Mode = AP_.GetMode();
  b.H.Publish(State_.NowS);

  FBAirframeBlock &a = State_.Airframe;
  a.GearPosition = (float)Ctrl_->GetGearPosition();
  /* A MOVER IS AIRBORNE BY CONSTRUCTION and says so here as well as to the mission judge: the NoOp
   * controls report false already, but stating it once in the block keeps the two answers one answer. */
  a.WeightOnWheels = Spec_.IsMover() ? false : Ctrl_->GetWeightOnWheels();
  a.SpeedbrakeNorm = (float)Ctrl_->GetSpeedbrake();
  a.EngineRunning = Spec_.IsMover() ? true : Ctrl_->GetEngineRunning(0);
  if (Fdm_) {
    double cap = Fdm_->GetFuelCapacityLbs();
    a.FuelLbs = (float)Fdm_->GetFuelTotalLbs();
    a.FuelPct = cap > 0.0 ? (float)(100.0 * a.FuelLbs / cap) : 0.0f;
  }
  a.H.Publish(State_.NowS);
}

/* THE NODE HALF of module.md §Spec 7, and the whole boundary is in what this function does NOT do: it
 * reads this aircraft's own nearest ANONYMOUS contact (no id, no team — FBRadarContact has neither),
 * reconstructs the POINT it sat at, and stamps the set's own look age on it. It cannot say hostile (no
 * such value exists), it cannot name a unit (FBNetReport has no id field), and it cannot hand anybody a
 * track. Killing its Radar stops it by the existing failure->Invalid coupling and not by a line here. */
void FBAirModule::PublishNetReport(const Fdm::fb_fdm_state &st) {
  /* Two ways onto a net and one derivation: the CATALOGUE row that is an early-warning aircraft, and
   * the MISSION `net` block that put this unit in one. The node's transmitted weapons-control word only
   * exists in the second case, so a catalogue node reports exactly what it always reported. */
  FBAirNet eff = Net_;
  eff.On = Net_.On || Spec_.NetNode;
  Node_ = FBBuildAirNetReport(eff, State_.Radar, Datalink_.Transmitting(), st.lat, st.lon, st.elev);
}

/* THE MEMBER HALF. Two command-bus entries per decision tick, each latency-charged and each rejectable:
 * an antenna AZIMUTH and an antenna ELEVATION. Not the range gate, not the frame time, not a track file
 * entry — the member's own radar must still detect, firm over kHitsToFirm looks and pass its own gate.
 * The cue's error is arithmetic and not policy: the node's own look age plus one net period at the
 * target's own speed, which is why a 400 km sensor never becomes a firing solution. */
void FBAirModule::ConsumeCue(const Fdm::fb_fdm_state &st) {
  const FBDatalinkBlock &b = State_.NetLink;
  HaveCue_ = false;
  CueAgeS_ = -1.0;
  if (!b.H.Readable() || b.TrackCount <= 0) return;
  /* ONE POINTING PER NET CYCLE, and not one per decision tick. A cue is a TRANSMISSION: re-issuing it
   * at 10 Hz would flood the command bus (measured: the elevation entry was rejected `channel_busy`
   * every second tick) and would claim a freshness the link does not have. */
  if (SimTimeS_ < NextCueS_) return;
  NextCueS_ = SimTimeS_ + Sensors::FBDatalinkSystem::kNetPeriodS;
  const FBDatalinkTrack *best = nullptr;
  for (int i = 0; i < b.TrackCount; i++)
    if (b.Tracks[i].Net.Reporting && (!best || b.Tracks[i].RangeM < best->RangeM))
      best = &b.Tracks[i];
  if (!best) return;

  double e = 0.0, n = 0.0;
  FBEnuOffsetM(st.lat, st.lon, best->Net.LatDeg, best->Net.LonDeg, e, n);
  double planar = std::sqrt(e * e + n * n);
  double brgDeg = std::atan2(e, n) * kRad2Deg;
  /* BOTH halves against this aircraft's own instruments. The cue names a POINT in the world; the
   * antenna is bolted to the nose, so the heading converts the one number and the pitch attitude the
   * other. Converting only the azimuth left the elevation carrying the aircraft's whole pitch angle —
   * the same defect as the MiG-29's, in the other axis (doc/pilot.md 2.15). */
  CueAzDeg_ = FBBodyAngle::FromTrueBearing(brgDeg, st.yaw).Deg();
  CueElDeg_ = FBBodyAngle::FromWorldElevation(
                  std::atan2(best->Net.AltM - st.elev, planar > 1.0 ? planar : 1.0) * kRad2Deg,
                  st.pitch).Deg();
  /* BOTH staleness terms, summed and published: the node's own look age at the moment it reported, plus
   * the age the receiving terminal computed. A mission can read which half hurt. */
  CueAgeS_ = (double)best->Net.TgtLookAgeS + (double)best->AgeS;
  HaveCue_ = true;

  Cmds_.PostAntennaAz(FBBodyAngle::Measured(CueAzDeg_), SimTimeS_);
  Cmds_.PostAntennaEl(FBBodyAngle::Measured(CueElDeg_), SimTimeS_);
  CueCount_++;
  FBLog::Info("net", "CUE", {{"azDeg", CueAzDeg_}, {"elDeg", CueElDeg_},
                             {"rangeM", planar}, {"ageS", CueAgeS_}});
}

/* The controller's call, converted where BOTH halves of each number finally exist: the bearing against
 * this aircraft's own heading, the elevation against its own altimeter AND its own pitch attitude. It
 * goes over the SAME command bus the live cue uses — two pointings, each latency-charged and each
 * rejectable — because a briefed call and a transmitted one are the same act, and neither is a track. */
void FBAirModule::EnterBriefedGci(const Fdm::fb_fdm_state &st) {
  GciPending_ = false;
  FBBodyAngle az = FBBodyAngle::FromTrueBearing(GciBrgDeg_, st.yaw);
  FBBodyAngle el = FBBodyAngle::FromWorldElevation(
      std::atan2(GciAltM_ - st.elev, GciRangeM_) * kRad2Deg, st.pitch);
  CueAzDeg_ = az.Deg();
  CueElDeg_ = el.Deg();
  Cmds_.PostAntennaAz(az, SimTimeS_);
  Cmds_.PostAntennaEl(el, SimTimeS_);
  CueCount_++;
  FBLog::Info("gci", "BRAA", {{"brgDeg", GciBrgDeg_}, {"rangeKm", GciRangeM_ * 0.001},
                              {"altM", GciAltM_}, {"ownHdgDeg", st.yaw}, {"ownPitchDeg", st.pitch},
                              {"azDeg", CueAzDeg_}, {"elDeg", CueElDeg_}});
}

void FBAirModule::Run(Fdm::fb_fdm_state &st, double dt, const Units::FBUnitRegistry *units,
                      const World::FBWorld *world) {
  (void)world;
  SimTimeS_ += dt;
  State_.NowS = SimTimeS_;
  Cmds_.SetLoadFactor(State_.AirData.H.Readable() ? State_.AirData.GLoad : 1.0f);

  if (Due(SensorAccS_, dt, 10.0)) {
    PublishPlatform(st);
    /* ...but not before the airframe has been integrated once: the boot hands the module a state that
     * carries position only, so on the very first tick this aircraft's own heading is still 0 and the
     * BRAA would be converted against a heading it is not flying. */
    if (GciPending_ && LastSub_ > 0) EnterBriefedGci(st);
    ServiceCommands(FBCommandGroup::Sensors);
    ServiceCommands(FBCommandGroup::Avionics);

    Radar_.SetRangeFactor(SystemDegraded(FBSystemId::Radar) ? kRadarRangeDegraded : 1.0);
    if (SystemWorking(FBSystemId::Radar) && Spec_.Radar.SearchRangeM > 0.0)
      Radar_.Run(State_, st, units, SimTimeS_);
    else
      State_.Radar.SetAbsent();
    if (Spec_.IrstRangeM > 0.0) Irst_.Run(State_, st, units, SimTimeS_);
    Eye_.Run(State_, st, units, SimTimeS_);   /* ungated: an eye is not a box the aeroplane carries */

    Rwr_.SetOwnRadiating(Radar_.Emission().Mode != FBEmitterMode::None);
    if (Spec_.HasRwr && SystemWorking(FBSystemId::Rwr)) Rwr_.Run(State_, st, units, SimTimeS_);
    else State_.Rwr.H.Invalidate();

    if (Datalink_.Powered()) {
      Datalink_.SetPowered(SystemWorking(FBSystemId::Datalink));
      Datalink_.Run(State_, st, units, SimTimeS_);
    }
    if (Spec_.NetMember) {
      NetLink_.SetPowered(SystemWorking(FBSystemId::Datalink));
      NetLink_.Run(State_, st, units, SimTimeS_);
      ConsumeCue(st);
    }
    FBRunAirNetMember(NetTerminalBlock(), Net_, Pilot_);
    PublishNetReport(st);

    AirData_.Run(State_, st, dt);
    RadarAlt_.Run(State_, (float)st.elev, GroundAslM_);
    if (const FBWaypoint *swp = Plan_.ActiveWaypoint())
      Nav_.SetSteerpoint(swp->LatDeg, swp->LonDeg, GroundAslM_ * kMToFt);
    Nav_.Run(State_, st, dt);

    ServiceCommands(FBCommandGroup::Stores);
    /* THE COMPUTER BEFORE THE RAILS, so the release interlock reads THIS tick's launch zone rather than
     * the previous one, and the round on the SELECTED station is the one the zone was solved for. */
    if (Fc_ && SystemWorking(FBSystemId::FireControl)) {
      Fc_->Run(State_, st, FBStoreSpecOf(Stores_.StoreAt(Stores_.SelectedStation())), Gun_.Spec(),
               SimTimeS_);
      Stores_.SetTargetState(Fc_->TargetState());
    } else {
      State_.FireControl.H.Invalidate();
      /* THE BINDING, and it costs one line: an invalid estimate stops the midcourse uplink, so a
       * semi-active round in flight loses its guidance the moment its shooter loses the track. */
      Stores_.SetTargetState(FBWeaponTargetState{});
    }
    if (SystemWorking(FBSystemId::Stores)) Stores_.Run(State_, dt);
    else State_.Stores.H.Invalidate();
    Warn_.Run(State_, dt);
  }
  if (SystemWorking(FBSystemId::Gun)) Gun_.Run(State_, st, SimTimeS_, dt);
  else State_.Gun.H.Invalidate();

  if (Due(PilotAccS_, dt, 10.0)) {
    ApplyPilotCommands(Pilot_.Run(State_, Cmds_, *Ctrl_, st, Plan_, HaveRunway_ ? &Rwy_ : nullptr, dt));
    if (!Spec_.IsMover()) Nav_.AdvanceWaypoint(Plan_, st.lat, st.lon);
  }

  if (Spec_.IsMover()) {
    /* THE KINEMATIC HALF. The mover IS the physics of this row; the pilot above it decided only whether
     * a reflex is running, which arrives here as a heading and nothing else. */
    if (Pilot_.Dragging()) Mover_.CommandHeading(Pilot_.DragHeadingDeg());
    else Mover_.ReleaseHeading();
    Mover_.Advance(st, Plan_, dt, SimTimeS_);
    LastSub_ = 0;
    return;
  }

  AccS_ += dt;
  LastSub_ = 0;
  for (int k = 0; Fdm_ && AccS_ >= Fdm::FBFdm::kStepS && k < 12; k++) {
    LastG_ = AP_.Run(st);
    Systems::FBControls c = FC_.Run(LastG_, st);
    Fdm_->SetControls(c.Roll, c.Pitch, c.Yaw, c.Thr);
    Fdm_->Step(st);
    AccS_ -= Fdm::FBFdm::kStepS;
    LastSub_++;
  }
}

void FBAirModule::ApplyPilotCommands(const Pilot::FBPilotCommands &c) {
  switch (c.Guidance) {
    case Pilot::FBPilotGuidance::Manual:
      AP_.SetManual(c.ManualRoll, c.ManualPitch, c.ManualYaw, c.ManualThr);
      break;
    case Pilot::FBPilotGuidance::Direct:
      if (c.HaveLeg)
        AP_.SetDirectLeg(c.LegLatDeg, c.LegLonDeg, c.TargetLatDeg, c.TargetLonDeg, c.TargetAltM,
                         c.TargetSpeedKt * kKtToMs);
      else
        AP_.SetDirect(c.TargetLatDeg, c.TargetLonDeg, c.TargetAltM, c.TargetSpeedKt * kKtToMs);
      break;
    case Pilot::FBPilotGuidance::Course:
      AP_.SetCourse(c.TargetLatDeg, c.TargetLonDeg, c.CourseDeg, c.TargetAltM, c.GlidepathDeg,
                    c.TargetSpeedKt * kKtToMs);
      break;
    case Pilot::FBPilotGuidance::None:
      break;
  }
  if (c.GearDown) Ctrl_->SetGear(*c.GearDown);
  if (c.Speedbrake) Ctrl_->SetSpeedbrake(*c.Speedbrake);
  if (c.WheelBrakeLeft || c.WheelBrakeRight)
    Ctrl_->SetWheelBrakes(c.WheelBrakeLeft.value_or(0.0), c.WheelBrakeRight.value_or(0.0));
  if (c.NosewheelSteer) Ctrl_->SetNosewheelSteer(*c.NosewheelSteer);
  if (c.EngineStart) *c.EngineStart ? Ctrl_->EngineStart() : Ctrl_->EngineCutoff();
}

void FBAirModule::ServiceCommands(FBCommandGroup group) {
  FBAvionicsCommand c{};
  while (Cmds_.TakeDue(group, SimTimeS_, c)) {
    FBCommandOutcome outcome = FBCommandOutcome::Accepted;
    FBCommandReason reason = FBCommandReason::None;
    ApplyCommand(c, outcome, reason);
    Cmds_.Complete(c, outcome, reason, SimTimeS_);
  }
}

/* Deliberately short. A key here names a box this aeroplane actually has; a catalogue cell has one
 * radar with one search pattern, a rail, a trigger and a transponder, and answering anything else would
 * report success for a setting with no system behind it. */
void FBAirModule::ApplyCommand(const FBAvionicsCommand &c, FBCommandOutcome &outcome,
                               FBCommandReason &reason) {
  auto reject = [&](FBCommandReason r) { outcome = FBCommandOutcome::Rejected; reason = r; };
  switch (c.Target) {
    case FBCommandTarget::RadarSlewAz:
      if (Spec_.Radar.SearchRangeM <= 0.0) { reject(FBCommandReason::OutOfContext); return; }
      Radar_.RecentreOn(c.Value, CueElDeg_);
      return;
    case FBCommandTarget::RadarSlewEl:
      if (Spec_.Radar.SearchRangeM <= 0.0) { reject(FBCommandReason::OutOfContext); return; }
      Radar_.RecentreOn(CueAzDeg_, c.Value);
      return;
    case FBCommandTarget::Designate:
      if (!SystemWorking(FBSystemId::Radar)) { reject(FBCommandReason::SystemFailed); return; }
      Radar_.SetSingleTarget(c.Value != 0.0);
      if (!Radar_.Designate((int)c.Value, SimTimeS_)) reject(FBCommandReason::OutOfContext);
      return;
    case FBCommandTarget::IffTransponder: Radar_.SetIffTransponder(c.Value != 0.0); return;
    case FBCommandTarget::IffInterrogator: Radar_.SetIffInterrogator(c.Value != 0.0); return;
    case FBCommandTarget::MasterArm:
      Stores_.SetMasterArm(c.Value != 0.0 ? FBArmState::Arm : FBArmState::Sim);
      Gun_.SetMasterArm(Stores_.MasterArm());
      return;
    case FBCommandTarget::WeaponSelect:
      if (!Stores_.SelectStation((int)c.Value)) reject(FBCommandReason::OutOfContext);
      return;
    case FBCommandTarget::WeaponRelease:
      if (!SystemWorking(FBSystemId::Stores)) { reject(FBCommandReason::SystemFailed); return; }
      Stores_.Release(SimTimeS_, outcome, reason);
      return;
    case FBCommandTarget::GunTrigger:
      if (!SystemWorking(FBSystemId::Gun)) { reject(FBCommandReason::SystemFailed); return; }
      Gun_.Trigger(c.Value, SimTimeS_, outcome, reason);
      return;
    default:
      reject(FBCommandReason::NotImplemented);
      return;
  }
}

bool FBAirModule::ApplySetup(const std::string &key, const std::string &value) {
  if (ApplyJammerSetup(key, value)) return true;

  /* THE RUNNER-GENERATED NET KEYS, never author-written: a `net` block is doctrine ABOUT units, and
   * core/FBMissionFile.cpp turns it into these. modules/FBAirNet.h. */
  { const char *why = nullptr;
    FBAirNetResult r = FBApplyAirNetKey(key, value, NetTerminal(), Pilot_, Net_, &why);
    if (r == FBAirNetResult::Ok) return true;
    if (r == FBAirNetResult::Bad) return RejectSetup(why, key, value); }

  /* ---- THE TWO NEW AUTHOR-FACING KEYS, and no more. Everything else about a row is said by its module
   * name; everything about its tasking uses a key that already exists. ---- */
  if (key == "orbit") {
    std::istringstream in(value);
    double legM = 0.0, periodS = 0.0;
    if (!(in >> legM >> periodS)) return RejectSetup("want '<legM> <periodS>'", key, value);
    if (legM <= 0.0 || periodS <= 0.0) return RejectSetup("want positive leg and period", key, value);
    if (!Spec_.IsMover()) return RejectSetup("an orbit is a MOVER's station; a deck row flies a route",
                                             key, value);
    Mover_.SetOrbit(legM, periodS);
    return true;
  }
  if (key == "drag_threat_s") {
    double s = 0.0;
    if (!ParseDouble(value, s)) return RejectSetup("not a number", key, value);
    if (s < 0.0) return RejectSetup("negative reflex duration", key, value);
    if (s > 0.0 && !Spec_.HasRwr)
      return RejectSetup("this row has no warning receiver, so it can never be triggered", key, value);
    if (s > 0.0 && Spec_.Tier != FBAirTier::Reflex)
      return RejectSetup("the drag reflex is the T1 tier's one state", key, value);
    Pilot_.SetDragThreatS(s);
    return true;
  }

  /* ---- THE TIER GATE, and it is a gate rather than an intention (module.md §Spec 5). A row may be
   * granted a tier only if its own MEASURED hooks support that tier's phases: doc/pilot.md's
   * close-combat law INVERTS an identified roll plant, and with another aircraft's plant it is not a
   * limiter but an oscillator. A generated deck has no plant until step 7 of the recipe measures one,
   * so a row whose plant is still zero is T0/T1 and `set task bfm` is REFUSED here rather than flown
   * badly. ---- */
  if (key == "task") {
    using Phase = Pilot::FBPilot::Phase;
    bool plant = Spec_.Perf.RollPlantA > 0.0 && Spec_.Perf.RollPlantKDegS > 0.0;
    if (value == "route") { Pilot_.SetPhase(Phase::Route); return true; }
    if (value == "orbit") {
      if (!Spec_.IsMover()) return RejectSetup("only a mover row holds an orbit", key, value);
      Pilot_.SetPhase(Phase::Orbit);
      return true;
    }
    if (value == "bfm") {
      if (Spec_.Tier < FBAirTier::Visual)
        return RejectSetup("this row's tier is T0/T1: it has no weapon and no close-combat phase",
                           key, value);
      if (!plant)
        return RejectSetup("this row's ROLL PLANT IS UNMEASURED — step 7 of the flight-model recipe has "
                           "not been run on its deck, and doc/pilot.md's close-combat law inverts the "
                           "plant, so with another airframe's it is an oscillator and not a limiter",
                           key, value);
      Pilot_.SetPhase(Phase::Bfm);
      return true;
    }
    if (value == "intercept") {
      if (Spec_.Tier < FBAirTier::Gci)
        return RejectSetup("intercept needs a radar and this row's tier has none", key, value);
      Pilot_.SetPhase(Phase::Intercept);
      return true;
    }
    if (value == "formation") {
      if (Spec_.Tier < FBAirTier::Peer)
        return RejectSetup("formation needs the cooperative half only a T4 row has", key, value);
      Pilot_.SetPhase(Phase::Formation);
      return true;
    }
    return RejectSetup("want route|orbit|bfm|intercept|formation", key, value);
  }

  /* ---- EVERYTHING ELSE REUSES A KEY THAT EXISTS. ---- */
  if (key == "gear") {
    if (value != "up" && value != "down") return RejectSetup("want up|down", key, value);
    Ctrl_->SetGear(value == "down");
    return true;
  }
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
    if (n < 0.0 || n != std::floor(n)) return RejectSetup("want a whole, non-negative count", key, value);
    if (!Gun_.SetRounds((int)n)) return RejectSetup("more rounds than the gun holds", key, value);
    return true;
  }
  if (key == "fcr") {
    if (value != "on" && value != "off") return RejectSetup("want on|off", key, value);
    if (Spec_.Radar.SearchRangeM <= 0.0)
      return RejectSetup("this row has no radar at all: it acquires through the eye", key, value);
    Radar_.SetPowered(value == "on");
    return true;
  }
  if (key == "rwr") {
    if (value != "on" && value != "off") return RejectSetup("want on|off", key, value);
    if (!Spec_.HasRwr) return RejectSetup("this row carries no warning receiver", key, value);
    Rwr_.SetPowered(value == "on");
    return true;
  }
  if (key == "iff_xpdr") {
    if (value != "on" && value != "off") return RejectSetup("want on|off", key, value);
    Radar_.SetIffTransponder(value == "on");
    return true;
  }
  if (key == "iff_interrogator") {
    if (value != "on" && value != "off") return RejectSetup("want on|off", key, value);
    Radar_.SetIffInterrogator(value == "on");
    return true;
  }
  if (key == "datalink") {
    if (value != "on" && value != "off") return RejectSetup("want on|off", key, value);
    if (Spec_.Tier != FBAirTier::Peer)
      return RejectSetup("only a T4 row carries a cooperative terminal", key, value);
    Datalink_.SetPowered(value == "on");
    return true;
  }
  if (key == "brief_sort") {
    Pilot::FBSortContract sc;
    if (!Pilot::FBSortContractFromString(value.c_str(), sc))
      return RejectSetup("want none|left|right|near|far", key, value);
    Pilot_.BriefSort(sc);
    return true;
  }
  if (key == "brief_master_arm") {
    if (value != "arm" && value != "sim") return RejectSetup("want arm|sim", key, value);
    Pilot_.BriefMasterArm(value == "arm");
    return true;
  }
  if (key == "brief_release_s") {
    double t = 0.0;
    if (!ParseDouble(value, t)) return RejectSetup("not a number", key, value);
    if (t < 0.0) return RejectSetup("negative time", key, value);
    if (!Pilot_.BriefRelease(t)) return RejectSetup("too many briefed releases", key, value);
    return true;
  }
  if (key == "brief_gun_s") {
    std::istringstream in(value);
    double atS = 0.0, burstS = 0.0;
    if (!(in >> atS >> burstS)) return RejectSetup("want '<atS> <burstS>'", key, value);
    if (atS < 0.0 || burstS <= 0.0) return RejectSetup("time < 0 or burst <= 0", key, value);
    if (!Pilot_.BriefGun(atS, burstS)) return RejectSetup("too many briefed bursts", key, value);
    return true;
  }
  /* THE CONTROLLER'S CALL AS MISSION DATA, and it goes through the SAME SEAM the live cue does — two
   * antenna pointings on the command bus, latency-charged and rejectable. Not a track, not a range
   * gate, not a designation. `set brief_gci <bearingDeg> <rangeKm> <altKm>`. */
  if (key == "brief_gci") {
    std::istringstream in(value);
    double brgDeg = 0.0, rangeKm = 0.0, altM = 0.0;
    if (!(in >> brgDeg >> rangeKm >> altM))
      return RejectSetup("want '<bearingDeg> <rangeKm> <altM>'", key, value);
    if (rangeKm <= 0.0) return RejectSetup("range <= 0", key, value);
    if (Spec_.Tier < FBAirTier::Gci)
      return RejectSetup("only a T3 row is a GCI interceptor", key, value);
    /* HELD, NOT ENTERED. A BRAA is a TRUE bearing and an ABSOLUTE altitude; an antenna takes a
     * BODY-relative azimuth and a RELATIVE elevation, and both conversions need this aircraft's own
     * heading and altimeter, which do not exist before it flies. Entering the raw numbers is what
     * air-bomber-intercept.fbm measured for a round: `set brief_gci 90 ...` parked a MiG-23's +-30 deg
     * antenna 90 deg off its own nose, nothing ever re-centred the azimuth, and an interceptor with a
     * 52 km set closed head-on to 11 km without one radar contact. */
    GciBrgDeg_ = brgDeg;
    GciRangeM_ = rangeKm * 1000.0;
    GciAltM_ = altM;
    GciPending_ = true;
    return true;
  }
  if (key == "fuel_pct") {
    double pct = 0.0;
    if (!ParseDouble(value, pct)) return RejectSetup("not a number", key, value);
    if (pct < 0.0 || pct > 100.0) return RejectSetup("want 0..100", key, value);
    if (!Fdm_) return RejectSetup("a mover row has no fuel system", key, value);
    Fdm_->SetFuelPct(pct);
    return true;
  }
  if (key.rfind("pilot_", 0) == 0) {
    double v = 0.0;
    if (!ParseDouble(value, v)) return RejectSetup("not a number", key, value);
    if (!Pilot_.ApplyTuning(key, v)) return RejectSetup("unknown pilot parameter", key, value);
    return true;
  }
  return false;   /* unknown key: the boot path logs SET_REJECTED with the key AND value */
}

} // namespace FlightBox::Modules
