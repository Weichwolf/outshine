/* FlightBox — FBUnitsStage: draw slot for AI-controlled units (future units/ registry), wired into
 * the scene pass right after the terrain draws. NoOp today — no unit registry exists yet (see
 * CLAUDE.md's units/ deferral) — but the App/FBRenderer already cycles this slot every frame, so a
 * real implementation only has to fill Encode(), never wire a new call site. */
#ifndef FBUNITSSTAGE_H
#define FBUNITSSTAGE_H

#include "FBDrawStage.h"

namespace FlightBox {

class FBUnitsStage : public FBDrawStage {
  /* Pure NoOp: FBDrawStage's default Init/Encode already do nothing. */
};

} // namespace FlightBox
#endif
