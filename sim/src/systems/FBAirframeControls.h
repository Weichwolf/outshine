/* FlightBox — FBAirframeControls: was die Haende des Piloten jenseits von Stick/Throttle und Guidance
 * anfassen (Interface + NoOp-Default), plus FBJsbsimAirframeControls als reale Ownship-Implementierung.
 * doc/flightbox/systems.md, Abschnitt 9. */
#ifndef FBAIRFRAMECONTROLS_H
#define FBAIRFRAMECONTROLS_H

#include "FBFdm.h"
#include "FBTelemetry.h"

namespace FlightBox::Systems {

class FBAirframeControls : public FBTelemetrySource {
public:
  virtual ~FBAirframeControls() = default;

  virtual void SetGear(bool down) { (void)down; }
  virtual void SetSpeedbrake(double norm) { (void)norm; }             /* 0..1 */
  virtual void SetWheelBrakes(double left, double right) { (void)left; (void)right; }   /* 0..1 each */
  virtual void SetNosewheelSteer(double norm) { (void)norm; }         /* -1..1 */
  virtual void EngineStart() {}
  virtual void EngineCutoff() {}

  /* Der EINZIGE Kanal, ueber den etwas oberhalb dieses Interface die Zelle wahrnehmen darf — das haelt
   * systems/ airframe- UND instanz-agnostisch. */
  virtual bool   GetWeightOnWheels() const { return false; }
  /* Nase am Boden = die Zwei-Punkt-Lage ist vorbei. Der Pilot fuehlt genau das, und die Prozedur
   * haengt daran (doc/f16/procedures-landing.md, Roll-Out), nicht an einer Geschwindigkeit. */
  virtual bool   GetNoseWheelOnGround() const { return false; }
  virtual double GetGearPosition() const { return 0.0; }   /* 0=ein .. 1=aus, kinematisch verzoegert */
  virtual double GetSpeedbrake() const { return 0.0; }     /* 0..1, verzoegerter Readback */
  virtual double GetGrossWeightLbs() const { return 0.0; }
  virtual bool   GetEngineRunning(int engineIndex) const { (void)engineIndex; return false; }

  /* Telemetrie ueber dieselben virtuellen Getter: gilt unveraendert fuer NoOp UND JSBSim-Ableitung. */
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
  explicit FBJsbsimAirframeControls(Fdm::FBFdm &fdm) : Fdm(fdm) {}

  void SetGear(bool down) override;
  void SetSpeedbrake(double norm) override;
  void SetWheelBrakes(double left, double right) override;
  void SetNosewheelSteer(double norm) override;
  void EngineStart() override;
  void EngineCutoff() override;

  bool   GetWeightOnWheels() const override;
  bool   GetNoseWheelOnGround() const override;
  double GetGearPosition() const override;
  double GetSpeedbrake() const override;
  double GetGrossWeightLbs() const override;
  bool   GetEngineRunning(int engineIndex) const override;

private:
  Fdm::FBFdm &Fdm;   /* geborgt — Besitzer ist, wem die Einheit gehoert */
};

} // namespace FlightBox::Systems
#endif
