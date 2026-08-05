#include "FBMissileModule.h"

namespace FlightBox::Modules {

FBMissileModule::FBMissileModule(const FBStoreSpec &spec) : Spec_(spec) {
  Guidance_.Bind(Spec_, Seeker_, Ir_, Ar_);
  Visual_.SetPowered(false);   /* a round carries no eyes */
  /* THREE OF THESE ARE THE ROUND, the rest are the telemetry column layout (FBStoreModule states why):
   * the seeker sits in the radar slot, the anti-radiation head in the warning-receiver slot and the
   * infrared head in the IRST slot, and the guidance IS this round's pilot. */
  DeclareAutopilot(AP_);
  DeclareFlightControl(FC_);
  DeclarePilotSystem(Guidance_);
  DeclareControls(Ctrl_);
  DeclareDisplays(Disp_);
  DeclareAirDataSystem(AirData_);
  DeclareNavSystem(Nav_);
  DeclareWarningSystem(Warn_);
  DeclareRadarAltimeter(RadarAlt_);
  DeclareCommands(Cmds_);
  DeclareDatalink(Uplink_);
  DeclareRadar(Seeker_);
  DeclareRwr(Ar_);
  DeclareIrst(Ir_);
  DeclareVisual(Visual_);
  DeclareCountermeasures(Cm_);
  DeclareStores(Stores_);
  DeclareGuns(Gun_);
  DeclareFlightPlan(Plan_);
}

void FBMissileModule::ProgramRelease(const FBStoreRelease &rel) {
  SimTimeS_ = rel.SimTimeS;   /* the round's clock IS mission time, seeded at separation */
  Guidance_.Program(rel.Target, rel.LauncherId, rel.SimTimeS);
  Guidance_.ProgramCue(rel.CueValid, rel.CueAzDeg, rel.CueElDeg, rel.ArClass);
  Uplink_.SetLauncherId(rel.LauncherId);
}

/* The uplink once, then fixed 100 Hz substeps of seeker -> guidance -> FCS -> FDM; same accumulator and
 * spiral guard as every other module. */
void FBMissileModule::Run(Fdm::fb_fdm_state &st, double dt, const Units::FBUnitRegistry *units,
                          const World::FBWorld *world) {
  (void)world;
  if (!Fdm_) return;
  Fdm::FBFdm &fdm = *Fdm_;
  SimTimeS_ += dt;
  State_.NowS = SimTimeS_;
  /* `units` reaches this slot and the seeker below, and nothing else in the module. */
  Uplink_.Run(State_, st, units, SimTimeS_);
  /* Not read by the guidance (which uses the accelerometer and gyros directly, like the real box), but
   * published so the trace carries the CAS/Mach/g the boost, coast and terminal turn are read off. */
  AirData_.Run(State_, st, dt);

  AccS_ += dt;
  LastSub_ = 0;
  for (int k = 0; AccS_ >= Fdm::FBFdm::kStepS && k < 12; k++) {
    double tS = SimTimeS_ - AccS_ + Fdm::FBFdm::kStepS;
    /* ONE seeker runs, and which one is the catalogue entry's — the other stays dark and publishes
     * nothing, so its block cannot be mistaken for a picture. */
    if (Spec_.Seeker == FBSeekerKind::Infrared) Ir_.Run(State_, st, units, tS);
    /* The passive head has no frame grid at all — it hears continuously, which is why it is the one
     * seeker that gets a fresh measurement on every substep. */
    else if (Spec_.Seeker == FBSeekerKind::AntiRadiation) Ar_.Run(State_, st, units, tS);
    else Seeker_.Run(State_, st, units, tS);   /* its antenna keeps its own frame grid on this clock */
    Guidance_.SetTime(tS);
    Pilot::FBPilotCommands g = Guidance_.Run(State_, Cmds_, Ctrl_, st, Plan_, nullptr, Fdm::FBFdm::kStepS);
    AP_.SetManual(g.ManualRoll, g.ManualPitch, g.ManualYaw, g.ManualThr);
    LastG_ = AP_.Run(st);
    Systems::FBControls c = FC_.Run(LastG_, st);
    fdm.SetControls(c.Roll, c.Pitch, c.Yaw, c.Thr);
    fdm.Step(st);
    AccS_ -= Fdm::FBFdm::kStepS;
    LastSub_++;
  }
}

} // namespace FlightBox::Modules
