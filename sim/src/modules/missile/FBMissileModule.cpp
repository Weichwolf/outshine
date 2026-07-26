#include "FBMissileModule.h"

namespace FlightBox {

FBMissileModule::FBMissileModule(const FBStoreSpec &spec) : Spec_(spec) {
  Guidance_.Bind(Spec_, Seeker_);
}

void FBMissileModule::ProgramRelease(const FBStoreRelease &rel) {
  SimTimeS_ = rel.SimTimeS;   /* the round's clock IS mission time, seeded at separation */
  Guidance_.Program(rel.Target, rel.LauncherId, rel.SimTimeS);
  Uplink_.SetLauncherId(rel.LauncherId);
}

/* One frame: the uplink once, then fixed 100 Hz substeps of seeker -> guidance -> FCS -> FDM. The
 * seeker and the guidance sit INSIDE the substep loop (header banner: a round covers 15 m per step) and
 * the pass-through autopilot/FCS carry the fin commands the guidance produced in that same step. Same
 * substep accumulator and spiral guard as every other module. */
void FBMissileModule::Run(fb_fdm_state &st, double dt, const FBUnitRegistry *units,
                          const FBWorld *world) {
  (void)world;
  if (!Fdm_) return;
  FBFdm &fdm = *Fdm_;
  SimTimeS_ += dt;
  State_.NowS = SimTimeS_;
  /* Comms: what the launcher is still telling this round, if anything (modules/missile/FBMissileUplink).
   * `units` reaches this slot and the seeker below, and nothing else in the module. */
  Uplink_.Run(State_, st, units, SimTimeS_);
  /* Air data: not used by the guidance (which reads the accelerometer and the gyros directly, like the
   * real box), but published so the round's own telemetry carries the CAS/Mach/g of every tick — the
   * columns the boost, the coast and the terminal turn are read off. */
  AirData_.Run(State_, st, dt);

  AccS_ += dt;
  LastSub_ = 0;
  for (int k = 0; AccS_ >= FBFdm::kStepS && k < 12; k++) {
    double tS = SimTimeS_ - AccS_ + FBFdm::kStepS;
    Seeker_.Run(State_, st, units, tS);   /* its antenna keeps its own frame grid on this clock */
    Guidance_.SetTime(tS);
    FBPilotCommands g = Guidance_.Run(State_, Cmds_, Ctrl_, st, Plan_, nullptr, FBFdm::kStepS);
    AP_.SetManual(g.ManualRoll, g.ManualPitch, g.ManualYaw, g.ManualThr);
    LastG_ = AP_.Run(st);
    FBControls c = FC_.Run(LastG_, st);
    fdm.SetControls(c.Roll, c.Pitch, c.Yaw, c.Thr);
    fdm.Step(st);
    AccS_ -= FBFdm::kStepS;
    LastSub_++;
  }
}

} // namespace FlightBox
