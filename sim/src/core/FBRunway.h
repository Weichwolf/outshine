/* The landing-relevant geometry of one runway: threshold, true heading (the extended centreline
 * course) and length/width for touchdown-zone and rollout checks. */
#ifndef FBRUNWAY_H
#define FBRUNWAY_H

namespace FlightBox {

struct FBRunway {
  double ThresholdLatDeg = 0.0, ThresholdLonDeg = 0.0, ThresholdElevM = 0.0;
  double TrueHeadingDeg = 0.0;
  double LengthM = 0.0, WidthM = 0.0;
};

} // namespace FlightBox
#endif
