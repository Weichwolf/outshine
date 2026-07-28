#include "FBMissileSeeker.h"

namespace FlightBox::Modules {

FBMissileSeeker::FBMissileSeeker() {
  Vol_.AzHalfDeg = kFovHalfDeg;
  Vol_.ElHalfDeg = kFovHalfDeg;
  Vol_.FrameS = kFrameS;
  Vol_.AutoAcquire = true;    /* nobody designates for a missile */
  Vol_.SingleTarget = true;   /* once it has one, all of it looks at that one */
  Vol_.Active = false;        /* dark until the guidance switches it on at the activation range */
  Vol_.RangeM = 0.0;
  SetIffInterrogator(false);  /* a missile carries no interrogator: it cannot ask who that is */
  SetIffTransponder(false);   /* and nothing answers on its behalf */
  Track_ = Vol_;
  Track_.AzCenterDeg = 0.0; Track_.ElCenterDeg = 0.0;
  Track_.AzHalfDeg = kGimbalHalfDeg;
  Track_.ElHalfDeg = kGimbalHalfDeg;
}

void FBMissileSeeker::SetRangeM(double m) {
  Vol_.RangeM = m;
  Track_.RangeM = m;
}

void FBMissileSeeker::SlewTo(double azDeg, double elDeg) {
  Vol_.AzCenterDeg = azDeg;
  Vol_.ElCenterDeg = elDeg;
}

} // namespace FlightBox::Modules
