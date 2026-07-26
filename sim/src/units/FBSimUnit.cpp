#include "FBSimUnit.h"
#include "FBElevationProvider.h"
#include "FBLog.h"
#include "FBUnits.h"
#include <cassert>

namespace FlightBox {

namespace {
FBFdm &RequireFdm(const std::unique_ptr<FBFdm> &fdm) {
  assert(fdm && "FBSimUnit needs a spawned airframe (FBFdmBoot::Spawn)");
  return *fdm;
}
} // namespace

FBSimUnit::FBSimUnit(int id, std::string name, FBUnitTeam team, std::unique_ptr<FBFdm> fdm,
                     std::unique_ptr<FBModule> module, const fb_fdm_state &initialState,
                     double groundAslM)
    : FBUnit(id, std::move(name), FBUnitKind::Aircraft, team),
      Fdm_(std::move(fdm)),
      Module_(std::move(module)),
      St_(initialState),
      GroundAslM_(groundAslM),
      FdmSrc_(RequireFdm(Fdm_), St_, GroundAslM_) {
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
}

void FBSimUnit::Run(double dt, const FBUnitRegistry *units, const FBWorld *world) {
  Module_->Run(St_, dt, units, world);
}

void FBSimUnit::UpdateGroundAsl(double sampleM) {
  if (FBElevationResolved(sampleM)) GroundAslM_ = sampleM;
  Module_->SetGroundAsl((float)GroundAslM_);
  Fdm_->SetGroundElevM(GroundAslM_);
}

FBState FBSimUnit::HudState() const {
  FBState hs = Module_->Telemetry();
  hs.roll = (float)St_.roll; hs.pitch = (float)St_.pitch; hs.yaw = (float)St_.yaw;
  hs.alt = (float)St_.elev; hs.gs = (float)St_.gs; hs.airspeed = (float)St_.speed; hs.vs = (float)St_.vy;
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
  Bus_.Register(&Module_->Datalink());   /* LAST: a new sensor appends columns, it never shifts old ones */
  Bus_.SetSink(sink);
  Bus_.Start();
}

} // namespace FlightBox
