#include "FBAirframeControls.h"
#include "jsbsim_adapter.h"

namespace FlightBox {

void FBJsbsimAirframeControls::SetGear(bool down) { fb_jsbsim_set_gear(down ? 1.0 : 0.0); }
void FBJsbsimAirframeControls::SetSpeedbrake(double norm) { fb_jsbsim_set_speedbrake(norm); }
void FBJsbsimAirframeControls::SetWheelBrakes(double left, double right) { fb_jsbsim_set_wheel_brakes(left, right); }
void FBJsbsimAirframeControls::SetNosewheelSteer(double norm) { fb_jsbsim_set_nosewheel_steer(norm); }
void FBJsbsimAirframeControls::EngineStart() { fb_jsbsim_engine_start(); }
void FBJsbsimAirframeControls::EngineCutoff() { fb_jsbsim_engine_cutoff(); }

/* unit<0 = model-wide gear/wow (see the adapter header) — the airframe-controls contract asks a single
 * yes/no question, not which gear; a per-gear breakdown belongs to a future landing-gear system. */
bool FBJsbsimAirframeControls::GetWeightOnWheels() const { return fb_jsbsim_get_wow(-1) != 0; }
double FBJsbsimAirframeControls::GetGearPosition() const { return fb_jsbsim_get_gear_pos(); }
double FBJsbsimAirframeControls::GetSpeedbrake() const { return fb_jsbsim_get_speedbrake_pos(); }

} // namespace FlightBox
