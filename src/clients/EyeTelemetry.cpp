#include "EyeTelemetry.h"

#include <cmath>

namespace outshine::Clients {
namespace {

const TelemetryChannel kChannels[] = {
    {"eyeLatDeg", "deg"},  {"eyeLonDeg", "deg"},  {"eyeAltAslM", "m"}, {"eyeEastM", "m"},
    {"eyeNorthM", "m"},    {"eyeTravelM", "m"},   {"eyeYawDeg", "deg"}, {"eyePitchDeg", "deg"}};

}

void EyeTelemetry::Moved(const Stance &s) {
  if (!Stood_) {
    Origin_ = TangentFrame::At(s.LatDeg, s.LonDeg);
    Stood_ = true;
  }
  double east = 0.0, north = 0.0;
  Origin_.Project(s.LatDeg, s.LonDeg, &east, &north);
  const double de = east - EastM_, dn = north - NorthM_;
  TravelM_ += std::sqrt(de * de + dn * dn);
  EastM_ = east;
  NorthM_ = north;
  At_ = s;
}

void EyeTelemetry::DeclareTelemetry(TelemetrySchema &schema) const {
  for (const TelemetryChannel &c : kChannels) schema.Add(c.Name, c.Unit);
}

void EyeTelemetry::SampleTelemetry(TelemetryRow &row) const {

  if (!Stood_) {
    for (size_t i = 0; i < sizeof kChannels / sizeof kChannels[0]; i++) row.Push(std::string());
    return;
  }
  row.Push(At_.LatDeg);
  row.Push(At_.LonDeg);
  row.Push(At_.AltAslM);
  row.Push(EastM_);
  row.Push(NorthM_);
  row.Push(TravelM_);
  row.Push(At_.YawDeg);
  row.Push(At_.PitchDeg);
}

}
