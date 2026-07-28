/* The draw slot for units, wired into the scene pass right after the terrain. NoOp today, but the
 * SLOT is real and cycled every frame — a real implementation only fills Encode(), it never has to
 * wire a new call site or change the pass count. */
#ifndef FBUNITSSTAGE_H
#define FBUNITSSTAGE_H

#include "FBDrawStage.h"

namespace FlightBox::Render {

class FBUnitsStage : public FBDrawStage {
  /* FBDrawStage's defaults already do nothing. */
};

} // namespace FlightBox::Render
#endif
