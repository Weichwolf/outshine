#include "FBOwnshipUnit.h"

namespace FlightBox {

FBUnitPose FBOwnshipUnit::GetPose() const {
  FBUnitPose p;
  p.LatDeg = State.lat; p.LonDeg = State.lon; p.ElevM = State.elev;
  p.RollDeg = State.roll; p.PitchDeg = State.pitch; p.YawDeg = State.yaw;
  p.SpeedMs = State.speed;
  p.HeadingDeg = State.yaw;   /* no separate ground-track field on fb_fdm_state; yaw is the FLCS-flown heading */
  return p;
}

} // namespace FlightBox
