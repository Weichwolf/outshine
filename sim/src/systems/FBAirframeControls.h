/* FlightBox — FBAirframeControls: what a pilot's HANDS touch beyond stick/throttle (FBFlightControl)
 * and guidance targets (FBAutopilot) — gear, speedbrake, wheel brakes, nosewheel steering, engine
 * start/cutoff. Interface + NoOp DEFAULT in one class (FBSystemSlots.h pattern): a module with no
 * airframe-controls path composes this unmodified.
 *
 * FBJsbsimAirframeControls below is the REAL implementation for the ownship: every setter forwards
 * straight to ONE BORROWED FBFdm (fdm/FBFdm.h) and every getter reads the SAME FDM property back, so a
 * kinematic gear transit or a WOW flip is visible the instant the FDM reports it — no shadow state kept
 * here. The FDM reference is CONSTRUCTOR-injected and never null: this object's association to one
 * airframe is fixed for its whole life (the module builds it when its FDM is attached), which is what
 * lets every method below be a plain forward with no "is there an FDM" branch. It is airframe-agnostic
 * (the FBFdm methods it uses are generic FGFCS/FGGroundReactions/FGPropulsion ties, see FBFdm.h), so any
 * JSBSim-flown module can reuse it, not only the F-16. */
#ifndef FBAIRFRAMECONTROLS_H
#define FBAIRFRAMECONTROLS_H

#include "FBFdm.h"
#include "FBTelemetry.h"

namespace FlightBox {

class FBAirframeControls : public FBTelemetrySource {
public:
  virtual ~FBAirframeControls() = default;

  virtual void SetGear(bool down) { (void)down; }
  virtual void SetSpeedbrake(double norm) { (void)norm; }             /* 0..1 */
  virtual void SetWheelBrakes(double left, double right) { (void)left; (void)right; }   /* 0..1 each */
  virtual void SetNosewheelSteer(double norm) { (void)norm; }         /* -1..1 */
  virtual void EngineStart() {}
  virtual void EngineCutoff() {}

  /* The airframe's own readbacks — the ONLY channel through which anything above this interface
   * (FBPilot above all) may sense the airframe. A pilot that reached past this into the FDM would be
   * bound to one concrete FDM implementation and to one instance of it; going through here keeps
   * systems/ airframe- AND instance-agnostic. */
  virtual bool   GetWeightOnWheels() const { return false; }
  virtual double GetGearPosition() const { return 0.0; }   /* 0=up .. 1=down, kinematic-lagged */
  virtual double GetSpeedbrake() const { return 0.0; }     /* 0..1, lagged readback */
  virtual double GetGrossWeightLbs() const { return 0.0; } /* live gross weight — FBPilot's rotation-speed table */
  virtual bool   GetEngineRunning(int engineIndex) const { (void)engineIndex; return false; }

  /* Telemetry via the same virtual getters every caller already uses — works unmodified for both the
   * NoOp default and FBJsbsimAirframeControls (no telemetry override needed in either). */
  const char *TelemetryName() const override { return "airframe"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override {
    schema.Add("gearPos"); schema.Add("wow"); schema.Add("speedbrake");
  }
  void SampleTelemetry(FBTelemetryRow &row) const override {
    row.Push(GetGearPosition()); row.Push(GetWeightOnWheels()); row.Push(GetSpeedbrake());
  }
};

class FBJsbsimAirframeControls : public FBAirframeControls {
public:
  explicit FBJsbsimAirframeControls(FBFdm &fdm) : Fdm(fdm) {}

  void SetGear(bool down) override;
  void SetSpeedbrake(double norm) override;
  void SetWheelBrakes(double left, double right) override;
  void SetNosewheelSteer(double norm) override;
  void EngineStart() override;
  void EngineCutoff() override;

  bool   GetWeightOnWheels() const override;
  double GetGearPosition() const override;
  double GetSpeedbrake() const override;
  double GetGrossWeightLbs() const override;
  bool   GetEngineRunning(int engineIndex) const override;

private:
  FBFdm &Fdm;   /* borrowed — owned by whoever owns the airframe (App/mission runner today) */
};

} // namespace FlightBox
#endif
