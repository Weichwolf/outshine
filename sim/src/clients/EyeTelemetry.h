/* WHERE THE EYE STOOD, IN EVERY ROW. A run whose whole subject is motion could not answer "did the
 * camera move" out of its own record, and that is the question every other reading rests on: a
 * frame distribution, a tile count and a stream ledger all mean something different standing still
 * than walking.
 *
 * THREE FRAMES, BECAUSE THEY ANSWER THREE QUESTIONS. The geodetic pair is the identity of the place
 * and the only form a tile server takes. East and north are metres in the tangent plane at the
 * standpoint the run started from, so a displacement is the subtraction of two doubles and no
 * reader needs a geodesic per row. Travel is the horizontal path length the eye actually walked,
 * summed at every move rather than between two rows — a circle back to the start is 0 m of
 * displacement and is not 0 m of walking, and a row every second cannot see the difference.
 *
 * THE LOOK IS YAW AND PITCH, not a unit vector: those are the two numbers a scene DECLARES and an
 * animation channel drives, so the row and the declaration are the same quantity in the same unit
 * and can be compared without a frame conversion. An ECEF triple would need this row's own latitude
 * read back before it meant anything.
 *
 * THE TANGENT PLANE IS EXACT THROUGH THE ELLIPSOID (core/TangentFrame.h) and flat, so over an arc
 * of s metres it under-reads by s^3/(6R^2) — 3 mm over 9 km, the longest declared run. It is taken
 * at ellipsoid height, which makes travel a ground track: climbing does not lengthen it. */
#ifndef EYETELEMETRY_H
#define EYETELEMETRY_H

#include "TangentFrame.h"
#include "Telemetry.h"

namespace outshine::Clients {

class EyeTelemetry : public TelemetrySource {
public:
  /* Degrees and metres above sea level, as the simulation resolved them — not as a channel asked
   * for them. A driven stance that never reaches the eye is exactly the defect this can see. */
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

} // namespace outshine::Clients
#endif
