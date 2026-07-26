/* FlightBox — FBAirframeControls: what a pilot's HANDS touch beyond stick/throttle (FBFlightControl)
 * and guidance targets (FBAutopilot) — gear, speedbrake, wheel brakes, nosewheel steering, engine
 * start/cutoff. Interface + NoOp DEFAULT in one class (FBSystemSlots.h pattern): a module with no
 * airframe-controls path composes this unmodified.
 *
 * FBJsbsimAirframeControls below is the REAL implementation for the ownship: every setter forwards
 * straight to the new JSBSim-adapter surface (fdm/jsbsim_adapter.h) and every getter reads the SAME
 * FDM property back, so a kinematic gear transit or a WOW flip is visible the instant the FDM reports
 * it — no shadow state kept here. It is airframe-agnostic (the adapter properties it uses are generic
 * FGFCS/FGGroundReactions/FGPropulsion ties, see the adapter header), so any JSBSim-flown module can
 * reuse it, not only the F-16. */
#ifndef FBAIRFRAMECONTROLS_H
#define FBAIRFRAMECONTROLS_H

namespace FlightBox {

class FBAirframeControls {
public:
  virtual ~FBAirframeControls() = default;

  virtual void SetGear(bool down) { (void)down; }
  virtual void SetSpeedbrake(double norm) { (void)norm; }             /* 0..1 */
  virtual void SetWheelBrakes(double left, double right) { (void)left; (void)right; }   /* 0..1 each */
  virtual void SetNosewheelSteer(double norm) { (void)norm; }         /* -1..1 */
  virtual void EngineStart() {}
  virtual void EngineCutoff() {}

  virtual bool   GetWeightOnWheels() const { return false; }
  virtual double GetGearPosition() const { return 0.0; }   /* 0=up .. 1=down, kinematic-lagged */
};

class FBJsbsimAirframeControls : public FBAirframeControls {
public:
  void SetGear(bool down) override;
  void SetSpeedbrake(double norm) override;
  void SetWheelBrakes(double left, double right) override;
  void SetNosewheelSteer(double norm) override;
  void EngineStart() override;
  void EngineCutoff() override;

  bool   GetWeightOnWheels() const override;
  double GetGearPosition() const override;
};

} // namespace FlightBox
#endif
