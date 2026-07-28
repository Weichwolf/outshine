#include "FBMig29Gun.h"

namespace FlightBox::Modules {

FBMig29Gun::FBMig29Gun() {
  Install(kGsh301, /*fwdM*/ 5.5, /*rightM*/ -0.75, /*downM*/ -0.2, /*boreDownDeg*/ 0.0,
          /*boreRightDeg*/ 0.0);
}

} // namespace FlightBox::Modules
