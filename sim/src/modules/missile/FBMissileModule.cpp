#include "FBMissileModule.h"

namespace FlightBox::Modules {

FBMissileModule::FBMissileModule(const FBStoreSpec &spec) : Spec_(spec) {
  Guidance_.Bind(Spec_, Seeker_);
  Rwr_.SetPowered(false);   /* a round carries no warning receiver — see the slot accessors */
}

void FBMissileModule::ProgramRelease(const FBStoreRelease &rel) {
  SimTimeS_ = rel.SimTimeS;   /* the round's clock IS mission time, seeded at separation */
  Guidance_.Program(rel.Target, rel.LauncherId, rel.SimTimeS);
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
    Seeker_.Run(State_, st, units, tS);   /* its antenna keeps its own frame grid on this clock */
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
