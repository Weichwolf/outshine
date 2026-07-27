#include "FBF16Gun.h"

namespace FlightBox {

FBF16Gun::FBF16Gun() {
  Install(kM61A1, /*fwdM*/ 4.6, /*rightM*/ -0.9, /*downM*/ -0.3, /*boreDownDeg*/ 0.0,
          /*boreRightDeg*/ 0.0);
}

} // namespace FlightBox
