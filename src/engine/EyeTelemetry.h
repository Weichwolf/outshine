#ifndef OUTSHINE_ENGINE_EYETELEMETRY_H
#define OUTSHINE_ENGINE_EYETELEMETRY_H

#include "TangentFrame.h"
#include "Telemetry.h"

namespace outshine::Clients {

class EyeTelemetry : public TelemetrySource {
public:

  struct Stance {
    double LatDeg = 0.0, LonDeg = 0.0, AltAslM = 0.0, YawDeg = 0.0, PitchDeg = 0.0;
  };

  void Moved(const Stance &s);

  double TravelM() const { return TravelM_; }
  double EastM() const { return EastM_; }
  double NorthM() const { return NorthM_; }

  const char *TelemetryName() const override { return "eye"; }
  void DeclareTelemetry(TelemetrySchema &schema) const override;
  void SampleTelemetry(TelemetryRow &row) const override;

private:
  TangentFrame Origin_;
  Stance At_;
  double EastM_ = 0.0, NorthM_ = 0.0, TravelM_ = 0.0;
  bool Stood_ = false;
};

}
#endif
