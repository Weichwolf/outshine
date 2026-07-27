/* The draw slot for effect billboards, wired into the scene pass right before the HUD. NoOp today,
 * but the SLOT is real and in the correct position — see FBUnitsStage. */
#ifndef FBSPRITESSTAGE_H
#define FBSPRITESSTAGE_H

#include "FBDrawStage.h"

namespace FlightBox {

class FBSpritesStage : public FBDrawStage {
  /* FBDrawStage's defaults already do nothing. */
};

} // namespace FlightBox
#endif
