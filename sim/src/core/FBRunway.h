/* FlightBox — FBRunway: the landing-relevant geometry of one runway — the threshold an approach aims
 * at, its true heading (extended centerline course), and physical length/width for touchdown-zone/
 * rollout checks. Value type in core/ (like FBFlightPlan) so FBPilot's approach/landing phases and any
 * future airfield database share the same shape without either owning the other. */
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
