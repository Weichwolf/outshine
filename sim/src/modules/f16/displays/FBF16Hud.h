/* FlightBox — FBF16Hud: the F-16's own MIL-STD-1787 symbology, the Displays override point. Pure
 * symbology: reads FBState, writes nothing. Layout and its sources: doc/modules-f16.md §12. */
#ifndef FBF16HUD_H
#define FBF16HUD_H

#include "FBDisplaySystem.h"

namespace FlightBox::Modules {

class FBF16Hud : public Systems::FBDisplaySystem {
public:
  void BuildHud(const FBState &state, const Systems::FBHudEnv &env, Systems::FBHudGeometry &out) const override;
};

} // namespace FlightBox::Modules
#endif
