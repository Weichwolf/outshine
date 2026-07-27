/* FlightBox — FBF16Hud: the F-16's own MIL-STD-1787 symbology, the Displays override point. Pure
 * symbology: reads FBState, writes nothing. Layout and its sources: doc/flightbox/modules-f16.md §12. */
#ifndef FBF16HUD_H
#define FBF16HUD_H

#include "FBDisplaySystem.h"

namespace FlightBox {

class FBF16Hud : public FBDisplaySystem {
public:
  void BuildHud(const FBState &state, const FBHudEnv &env, FBHudGeometry &out) const override;
};

} // namespace FlightBox
#endif
