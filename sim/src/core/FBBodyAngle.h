/* An angle that is BODY-REFERENCED, as a type rather than as a comment. Three times in three rounds a
 * WORLD-frame number was written into a body-frame command (doc/pilot.md 2.15, doc/journal.md), and
 * every one of them compiled: both frames are `double`, both are degrees, and the two spellings differ
 * only in a subtraction nobody could see missing. So the subtraction became the CONSTRUCTOR.
 *
 * A value of this type can be obtained in exactly three ways, and each one names its provenance:
 *   FromTrueBearing   a rechtweisende Peilung, converted against this aircraft's own heading
 *   FromWorldElevation  an elevation above the local horizontal, against its own pitch attitude
 *   Measured          a sensor that already reports off the nose (FBRadarContact::AzDeg/ElDeg,
 *                     FBRwrThreat::BearingDeg, FBIrstContact::AzDeg) — the ONE escape hatch, and it is
 *                     named so that an unearned use is visible at the call site
 * There is no syntax for "just take this double", which is the point: core/FBCommandBus's antenna
 * entries take this type and nothing else, and tools/verify_layers.py counts the files that may name a
 * slew target at all (today: one, the bus itself).
 *
 * THE ARITHMETIC IS THE ANTENNA KNOB's, not a full rotation: azimuth off the nose is the bearing minus
 * the heading, elevation off the boresight plane is the angle above the horizon minus the pitch. It is
 * exact at zero roll and on the nose, and it is the same arithmetic pilot/FBPilot's own uncued search
 * law uses — the two must agree or one steers where the other reports. The EXACT transform, for a
 * caller that has a whole line-of-sight vector rather than a controller's two numbers, is
 * FBGeodesy.h's FBEnuToBodyLos. */
#ifndef FBBODYANGLE_H
#define FBBODYANGLE_H

#include "FBGeodesy.h"

namespace FlightBox {

class FBBodyAngle {
public:
  FBBodyAngle() = default;   /* the nose / the boresight plane */

  static FBBodyAngle FromTrueBearing(double trueDeg, double ownYawDeg) {
    return FBBodyAngle(FBWrap180(trueDeg - ownYawDeg));
  }
  /* Deliberately unwrapped: an elevation lives in [-90,90] and folding it would turn a dish driven into
   * its stop into a dish pointing backwards. */
  static FBBodyAngle FromWorldElevation(double worldElDeg, double ownPitchDeg) {
    return FBBodyAngle(worldElDeg - ownPitchDeg);
  }
  static FBBodyAngle Measured(double bodyDeg) { return FBBodyAngle(bodyDeg); }

  double Deg() const { return Deg_; }

private:
  explicit FBBodyAngle(double deg) : Deg_(deg) {}
  double Deg_ = 0.0;
};

} // namespace FlightBox
#endif
