#include "FBAirframeControls.h"

namespace FlightBox::Systems {

void FBJsbsimAirframeControls::SetGear(bool down) { Fdm.SetGear(down ? 1.0 : 0.0); }
void FBJsbsimAirframeControls::SetSpeedbrake(double norm) { Fdm.SetSpeedbrake(norm); }
void FBJsbsimAirframeControls::SetWheelBrakes(double left, double right) { Fdm.SetWheelBrakes(left, right); }
void FBJsbsimAirframeControls::SetNosewheelSteer(double norm) { Fdm.SetNosewheelSteer(norm); }
void FBJsbsimAirframeControls::EngineStart() { Fdm.EngineStart(); }
void FBJsbsimAirframeControls::EngineCutoff() { Fdm.EngineCutoff(); }

/* Modellweites WOW: eine Ja/Nein-Frage, keine Aufschluesselung je Fahrwerk. */
bool FBJsbsimAirframeControls::GetWeightOnWheels() const { return Fdm.GetWow(); }
bool FBJsbsimAirframeControls::GetNoseWheelOnGround() const { return Fdm.GetNoseGearOnGround(); }
double FBJsbsimAirframeControls::GetGearPosition() const { return Fdm.GetGearPos(); }
double FBJsbsimAirframeControls::GetSpeedbrake() const { return Fdm.GetSpeedbrakePos(); }
double FBJsbsimAirframeControls::GetGrossWeightLbs() const { return Fdm.GetWeightLbs(); }
bool FBJsbsimAirframeControls::GetEngineRunning(int engineIndex) const { return Fdm.GetEngineRunning(engineIndex); }

} // namespace FlightBox::Systems
