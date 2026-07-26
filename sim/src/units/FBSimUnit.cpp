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

FBFdm &RequireFdm(const std::unique_ptr<FBFdm> &fdm) {
  assert(fdm && "FBSimUnit needs a spawned airframe (FBFdmBoot::Spawn)");
  return *fdm;
}
} // namespace

FBSimUnit::FBSimUnit(int id, std::string name, FBUnitKind kind, FBUnitTeam team,
                     std::unique_ptr<FBFdm> fdm, std::unique_ptr<FBModule> module,
                     const fb_fdm_state &initialState, double groundAslM)
    : FBUnit(id, std::move(name), kind, team),
      Fdm_(std::move(fdm)),
      Module_(std::move(module)),
      St_(initialState),
      GroundAslM_(groundAslM),
      FdmSrc_(RequireFdm(Fdm_), St_, GroundAslM_),
      BusSrc_(Module_->Telemetry()) {
  assert(Module_ && "FBSimUnit needs the module that flies the airframe");
  Module_->SetUnitIdentity(GetId(), GetTeam());   /* before the first Run: the terminal's own callsign
                                                   * on the net (FBModule::SetUnitIdentity) */
  PublishPose();   /* the declarative spawn is already a valid pose — nobody ever reads an empty one */
}

void FBSimUnit::PublishPose() {
  Pose_.LatDeg = St_.lat; Pose_.LonDeg = St_.lon; Pose_.ElevM = St_.elev;
  Pose_.RollDeg = St_.roll; Pose_.PitchDeg = St_.pitch; Pose_.YawDeg = St_.yaw;
  Pose_.SpeedMs = St_.speed;
  Pose_.HeadingDeg = St_.yaw;   /* no ground-track field on fb_fdm_state; yaw is the flown heading */
  Sig_.DatalinkXmt = Module_->Datalink().Transmitting();
  Sig_.IffXpdr = Module_->Radar().IffTransponder();   /* what another jet's interrogator can get back */
}

void FBSimUnit::Run(double dt, const FBUnitRegistry *units, const FBWorld *world) {
  Module_->Run(St_, dt, units, world);
}

void FBSimUnit::UpdateGroundAsl(double sampleM) {
  if (FBElevationResolved(sampleM)) GroundAslM_ = sampleM;
  Module_->SetGroundAsl((float)GroundAslM_);
  Fdm_->SetGroundElevM(GetKind() == FBUnitKind::Weapon ? kWeaponNoGroundElevM : GroundAslM_);
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

bool FBSimUnit::RunMonitors(double simT) {
  if (Flight_.Tick(FBBuildFlightMonitorSample(*Fdm_, St_, GroundAslM_), simT)) {
    Module_->Controls().EngineCutoff();
    return true;
  }
  if (Mission_ && Mission_->Tick({St_.lat, St_.lon, Fdm_->GetWow(), St_.gs * kMsToKt}, simT)) {
    if (Mission_->Verdict() == FBMissionVerdict::Fail)
      Module_->Controls().EngineCutoff();   /* touched down off the assigned runway */
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
  Bus_.SetSink(sink);
  Bus_.Start();
}

} // namespace FlightBox
