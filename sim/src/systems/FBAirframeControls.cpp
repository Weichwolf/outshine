#include "FBAirframeControls.h"

namespace FlightBox {

void FBJsbsimAirframeControls::SetGear(bool down) { Fdm.SetGear(down ? 1.0 : 0.0); }
void FBJsbsimAirframeControls::SetSpeedbrake(double norm) { Fdm.SetSpeedbrake(norm); }
void FBJsbsimAirframeControls::SetWheelBrakes(double left, double right) { Fdm.SetWheelBrakes(left, right); }
void FBJsbsimAirframeControls::SetNosewheelSteer(double norm) { Fdm.SetNosewheelSteer(norm); }
void FBJsbsimAirframeControls::EngineStart() { Fdm.EngineStart(); }
void FBJsbsimAirframeControls::EngineCutoff() { Fdm.EngineCutoff(); }

/* Model-wide WOW (FBFdm::GetWow) — the airframe-controls contract asks a single yes/no question, not
 * which gear; a per-gear breakdown belongs to a future landing-gear system. */
bool FBJsbsimAirframeControls::GetWeightOnWheels() const { return Fdm.GetWow(); }
double FBJsbsimAirframeControls::GetGearPosition() const { return Fdm.GetGearPos(); }
double FBJsbsimAirframeControls::GetSpeedbrake() const { return Fdm.GetSpeedbrakePos(); }
double FBJsbsimAirframeControls::GetGrossWeightLbs() const { return Fdm.GetWeightLbs(); }
bool FBJsbsimAirframeControls::GetEngineRunning(int engineIndex) const { return Fdm.GetEngineRunning(engineIndex); }

} // namespace FlightBox
