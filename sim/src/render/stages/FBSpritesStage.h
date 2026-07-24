/* FlightBox — FBSpritesStage: draw slot for future effect billboards (flares, smoke, ...), wired into
 * the scene pass right before the HUD pass. NoOp today — no effect system exists yet — but the slot
 * is real: FBRenderer already cycles it every frame in the correct position, so a real implementation
 * only has to fill Encode(), never wire a new call site. */
#ifndef FBSPRITESSTAGE_H
#define FBSPRITESSTAGE_H

#include "FBDrawStage.h"

namespace FlightBox {

class FBSpritesStage : public FBDrawStage {
  /* Pure NoOp: FBDrawStage's default Init/Encode already do nothing. */
};

} // namespace FlightBox
#endif
