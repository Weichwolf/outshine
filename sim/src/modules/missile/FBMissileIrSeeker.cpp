#include "FBMissileIrSeeker.h"

namespace FlightBox::Modules {

FBMissileIrSeeker::FBMissileIrSeeker() {
  Field_.FrameS = 0.05;        /* a STARE at 20 looks/s, the radar seeker's figure and its reason */
  Field_.AutoAcquire = true;   /* nobody designates for a missile */
  Field_.SingleTarget = false; /* a passive head keeps its whole field while it follows one mark */
  Field_.Active = false;       /* caged until the guidance uncages it */
  Track_ = Field_;
  Configure(0.0, 0.0, 0.0);
  /* NOT powered here: a round whose catalogue entry names a different seeker must not publish an
   * infrared block at all, or its trace would claim a detector it does not carry. The guidance powers
   * this head, and only for an FBSeekerKind::Infrared round. */
}

void FBMissileIrSeeker::Configure(double fovHalfDeg, double gimbalHalfDeg, double rangeM) {
  Field_.AzHalfDeg = fovHalfDeg;
  Field_.ElHalfDeg = fovHalfDeg;
  Field_.RangeM = rangeM;
  Track_.AzCenterDeg = 0.0;
  Track_.ElCenterDeg = 0.0;
  Track_.AzHalfDeg = gimbalHalfDeg;
  Track_.ElHalfDeg = gimbalHalfDeg;
  Track_.RangeM = rangeM;
  Track_.FrameS = Field_.FrameS;
  Track_.AutoAcquire = Field_.AutoAcquire;
  Track_.SingleTarget = Field_.SingleTarget;
  Track_.Active = Field_.Active;
}

void FBMissileIrSeeker::SlewTo(double azDeg, double elDeg) {
  Field_.AzCenterDeg = azDeg;
  Field_.ElCenterDeg = elDeg;
}

void FBMissileIrSeeker::SetUncaged(bool on) {
  Field_.Active = on;
  Track_.Active = on;
}

} // namespace FlightBox::Modules
