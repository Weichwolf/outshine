#include "FBSimUnit.h"
#include "FBElevationProvider.h"
#include "FBLog.h"
#include "FBUnits.h"
#include <cassert>

namespace FlightBox {

namespace {
/* A WEAPON's airframe is deliberately given no ground to collide with. JSBSim's ground reactions model
 * an object RESTING on a surface — the mk82 model's two STRUCTURE contacts are a 10,000 lbf/ft spring
 * with 200,000 lbf/ft/s damping, which is what keeps a parked store from sinking into the tarmac and
 * which, met at 150 m/s, is a stiff ODE that diverges inside a single step (measured: the integration
 * blows up on the contact step, so there is no impact state left to report). A store does not bounce:
 * it detonates. So its flight is pure aero and gravity from release to impact, and WHERE that impact is
 * stays the judge's call — core/FBFlightMonitor tests the same real elevation sample against the same
 * penetration rule it applies to every other unit (CLAUDE.md "Kein Cheaten": the module never decides
 * that its own flight is over). Far below any trajectory, and finite, so nothing downstream sees an
 * infinity. */
constexpr double kWeaponNoGroundElevM = -100000.0;

} // namespace

FBSimUnit::FBSimUnit(int id, std::string name, FBUnitKind kind, FBUnitTeam team,
                     std::unique_ptr<FBFdm> fdm, std::unique_ptr<FBModule> module,
                     const fb_fdm_state &initialState, double groundAslM)
    : FBUnit(id, std::move(name), kind, team),
      Fdm_(std::move(fdm)),
      Module_(std::move(module)),
      St_(initialState),
      GroundAslM_(groundAslM),
      FdmSrc_(Fdm_.get(), St_, GroundAslM_),
      BusSrc_(Module_->Telemetry()),
      HealthSrc_(Health_) {
  assert(Module_ && "FBSimUnit needs the module that flies the airframe");
  Module_->SetUnitIdentity(GetId(), GetTeam());   /* before the first Run: the terminal's own callsign
                                                   * on the net (FBModule::SetUnitIdentity) */
  Module_->AttachHealth(Health_);   /* read-only: the module sees its damage, never writes it */
  PublishPose();   /* the declarative spawn is already a valid pose — nobody ever reads an empty one */
}

void FBSimUnit::PublishPose() {
  Pose_.LatDeg = St_.lat; Pose_.LonDeg = St_.lon; Pose_.ElevM = St_.elev;
  Pose_.RollDeg = St_.roll; Pose_.PitchDeg = St_.pitch; Pose_.YawDeg = St_.yaw;
  Pose_.SpeedMs = St_.speed;
  Pose_.HeadingDeg = St_.yaw;   /* no ground-track field on fb_fdm_state; yaw is the flown heading */
  Sig_.DatalinkXmt = Module_->Datalink().Transmitting();
  Sig_.IffXpdr = Module_->Radar().IffTransponder();   /* what another jet's interrogator can get back */
  /* THE RADAR BEAM (core/FBEmitter.h): what the set is radiating and where it is pointed, derived by the
   * set itself so the emission can never disagree with the pattern the antenna is flying. The ONE thing
   * added here is the guidance case: a tracking radar that is ALSO supporting a shot is a different
   * warning to be on the receiving end of (doc/f16/defence-rwr-cm.md §1's flashing circle), and whether
   * this jet is supporting one is the STORES system's knowledge, not the radar's — so the two published
   * emissions are combined at the barrier that publishes both, and neither system learns about the
   * other. */
  Sig_.Radar = Module_->Radar().Emission();
  if (Sig_.Radar.Mode == FBEmitterMode::Track && Module_->Stores().Uplink().Active)
    Sig_.Radar.Mode = FBEmitterMode::Guidance;
  /* ...and what this jet has thrown into the air behind it (systems/FBCountermeasureSystem). */
  const FBChaffCloud *clouds = Module_->Countermeasures().Clouds();
  for (int i = 0; i < kMaxChaffClouds; i++) Sig_.Chaff[i] = clouds[i];
  /* The midcourse guidance uplink to a round this unit launched (systems/FBStoresSystem::Uplink) — an
   * emission like the two above, published at the same barrier so no missile ever reads its launcher's
   * transmitter half-way through a tick. */
  Sig_.Uplink = Module_->Stores().Uplink();
}

void FBSimUnit::Run(double dt, const FBUnitRegistry *units, const FBWorld *world) {
  Module_->Run(St_, dt, units, world);
}

void FBSimUnit::UpdateGroundAsl(double sampleM) {
  if (FBElevationResolved(sampleM)) GroundAslM_ = sampleM;
  Module_->SetGroundAsl((float)GroundAslM_);
  if (Fdm_) Fdm_->SetGroundElevM(GetKind() == FBUnitKind::Weapon ? kWeaponNoGroundElevM : GroundAslM_);
}

/* The module's bus with THIS frame's pose folded in: the module publishes the platform block at its own
 * slot cadence, the client re-publishes it at frame rate so the conformal HUD is drawn against the pose
 * actually being rendered. Same block, same writer role — not a second copy of the truth. */
FBState FBSimUnit::HudState() const {
  FBState hs = Module_->Telemetry();
  FBPlatformBlock &p = hs.Platform;
  p.RollDeg = (float)St_.roll; p.PitchDeg = (float)St_.pitch; p.YawDeg = (float)St_.yaw;
  p.AltM = (float)St_.elev; p.GsMs = (float)St_.gs; p.TasMs = (float)St_.speed; p.VsMs = (float)St_.vy;
  p.H.Publish(hs.NowS);
  return hs;
}

/* The burst, resolved and then FLOWN. Two steps, in this order and nowhere else: the core decides what
 * the geometry did (this unit's module supplies only WHERE its systems are), and the result is handed to
 * the airframe. Everything after that is ordinary simulation — the module cycles the systems it has
 * left, JSBSim integrates the aircraft it now is, and both monitors go on judging exactly as before. */
FBDamageResult FBSimUnit::TakeBurst(const FBBurst &burst) {
  FBDamageResult r = FBDamageModel::Apply(burst, Module_->DamageLayout(), Health_);
  ApplyDamageToAirframe();
  return r;
}

FBDamageResult FBSimUnit::TakeKineticBurst(const FBKineticBurst &burst) {
  FBDamageResult r = FBDamageModel::ApplyKinetic(burst, Module_->DamageLayout(), Health_);
  ApplyDamageToAirframe();
  return r;
}

/* Health -> physics (core/FBDamageModel's consequence constants). Idempotent and called only when the
 * register changed: nothing here is per-frame work. */
void FBSimUnit::ApplyDamageToAirframe() {
  /* No airframe, no physics to push it into: a wrecked ground target is wrecked in its register and
   * nowhere else, which is the whole of what "destroyed" means for something that never moved. */
  if (!Fdm_) return;
  switch (Health_.State(FBSystemId::Engine)) {
    case FBHealthState::Failed:
      Module_->Controls().EngineCutoff();   /* through the same controls path a pilot would use */
      Fdm_->SetThrottleLimit(0.0);
      break;
    case FBHealthState::Degraded: Fdm_->SetThrottleLimit(kThrottleLimitDegraded); break;
    case FBHealthState::Intact: break;
  }
  switch (Health_.State(FBSystemId::FlightControls)) {
    case FBHealthState::Failed: Fdm_->SetControlAuthority(kAuthorityFailed); break;
    case FBHealthState::Degraded: Fdm_->SetControlAuthority(kAuthorityDegraded); break;
    case FBHealthState::Intact: break;
  }
  switch (Health_.State(FBSystemId::Structure)) {
    case FBHealthState::Failed: Fdm_->SetDamageDrag(kDamageDragFt2Failed); break;
    case FBHealthState::Degraded: Fdm_->SetDamageDrag(kDamageDragFt2Degraded); break;
    case FBHealthState::Intact: break;
  }
}

/* The mission judge's whole per-tick input, in one place: this unit's observed position/contact/speed,
 * the one bit its own health register publishes, and the roster of the others (core/FBObjective.h). */
FBMissionMonitorSample FBSimUnit::BuildMissionSample(const FBMissionRoster &roster) const {
  FBMissionMonitorSample s;
  s.LatDeg = St_.lat; s.LonDeg = St_.lon;
  s.AnyWow = Fdm_ ? Fdm_->GetWow() : true;   /* a unit with no airframe is on the ground, by definition */
  s.GroundSpeedKt = St_.gs * kMsToKt;
  s.CombatIneffective = !Health_.CombatEffective();
  s.Roster = roster;
  return s;
}

bool FBSimUnit::FinalizeMission(double simT, const FBMissionRoster &roster) {
  return Mission_ && Mission_->Finalize(BuildMissionSample(roster), simT);
}

bool FBSimUnit::RunMonitors(double simT, const FBMissionRoster &roster) {
  /* THE PHYSICS JUDGE ONLY JUDGES PHYSICS. Without an airframe there is no flight to lose control of
   * and no structure to break — every input the monitor takes is an FDM observation — so a unit that
   * does not fly is simply never shown to it. Its MISSION judge below is untouched: what a static unit
   * has to achieve, if anything, is still judged exactly as every other unit's is. */
  if (Fdm_ && Flight_.Tick(FBBuildFlightMonitorSample(*Fdm_, St_, GroundAslM_), simT)) {
    Module_->Controls().EngineCutoff();
    return true;
  }
  if (Mission_ && Mission_->Tick(BuildMissionSample(roster), simT)) {
    /* Touched down off the assigned runway. NOT for a shootdown (the other way this verdict is reached):
     * a destroyed jet's engine is whatever the damage left it as, and cutting it here would be the
     * verdict acting on the aircraft instead of the damage doing so. */
    if (Mission_->Verdict() == FBMissionVerdict::Fail && Health_.CombatEffective())
      Module_->Controls().EngineCutoff();
    return true;
  }
  return false;
}

void FBSimUnit::CheckEnvelope() {
  /* AoA is numerically undefined at near-zero airspeed — the gate avoids a settle-transient warning. */
  bool haveAirspeed = St_.cas > 15.0;
  if (haveAirspeed && St_.alphaDeg > 25.0 && !WarnedStall_) {
    FBLog::Warn("pilot", "WARN", {{"kind", "stall"}, {"aoaDeg", St_.alphaDeg}});
    WarnedStall_ = true;
  } else if (!haveAirspeed || St_.alphaDeg < 20.0) WarnedStall_ = false;
  if (St_.mach > 1.2 && !WarnedOverspeed_) {
    FBLog::Warn("pilot", "WARN", {{"kind", "overspeed"}, {"mach", St_.mach}});
    WarnedOverspeed_ = true;
  } else if (St_.mach < 1.1) WarnedOverspeed_ = false;
  if (AglM() < 150.0 && St_.vy < -15.0 && !WarnedSink_) {
    FBLog::Warn("pilot", "WARN", {{"kind", "sink"}, {"vsMs", St_.vy}, {"aglM", AglM()}});
    WarnedSink_ = true;
  } else if (St_.vy > -5.0) WarnedSink_ = false;
}

void FBSimUnit::StartTelemetry(FBTelemetrySink *sink) {
  Bus_.Register(&FdmSrc_);
  Bus_.Register(&Module_->AirDataSystem());
  Bus_.Register(&Module_->PilotSystem());
  Bus_.Register(&Module_->FlightControl());
  Bus_.Register(&Module_->Controls());
  Bus_.Register(&Module_->Datalink());
  Bus_.Register(&Module_->Radar());
  /* LAST: a new source appends columns, it never shifts old ones. The pilot's BFM picture/scoreboard is
   * its own source rather than more of the pilot's channels for exactly that reason (systems/FBBfmTrack
   * — the pilot's own channels sit in the middle of every telemetry.csv ever measured). */
  Bus_.Register(&Module_->PilotSystem().BfmTrack());
  /* Then the two channels this round adds, in this order and after everything above for the same
   * reason: the warning set (what the jet is annunciating, including what it CANNOT evaluate because a
   * source block is invalid) and the command stream (what the pilot operated, and what the jet said
   * back). Appending is what keeps every column ever measured where it was. */
  Bus_.Register(&Module_->WarningSystem());
  Bus_.Register(&Module_->Commands());
  Bus_.Register(&BusSrc_);
  /* LAST again, and for the same appending rule: the stores books (systems/FBStoresSystem) — what is
   * loaded, what was released, and the gross weight that follows from it. */
  Bus_.Register(&Module_->Stores());
  /* And LAST once more, for the third time and the same reason: the defensive pair. Their two blocks
   * were added to FBState after core/FBStateBusTelemetry's list had already been measured in the middle
   * of every telemetry.csv, so each of them carries its own block-validity column here at the end
   * rather than moving every column to the right of that list (see FBStateBusTelemetry's banner). */
  Bus_.Register(&Module_->Rwr());
  Bus_.Register(&Module_->Countermeasures());
  /* And LAST once more, same rule: the intercept's state machine and its debrief (systems/
   * FBEngagement) — the pilot's THIRD source, appended so that every column measured before it stays
   * exactly where it was. */
  Bus_.Register(&Module_->PilotSystem().Engagement());
  /* And LAST once more, same appending rule: what this unit has had shot off it (core/FBSystemHealth).
   * It is the unit's own source, not a system's — the register belongs to the unit and no module may
   * even write it. */
  Bus_.Register(&HealthSrc_);
  /* And LAST once more — literally last, AFTER the health register, same appending rule: the gun's books
   * (systems/FBGunSystem). Registering it anywhere earlier would have moved the dmg_* columns, which
   * every damage measurement ever taken reads by position. */
  Bus_.Register(&Module_->Guns());
  Bus_.SetSink(sink);
  Bus_.Start();
}

} // namespace FlightBox
