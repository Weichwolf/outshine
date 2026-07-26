/* FlightBox — FBOwnshipUnit: the player's own jet, seen through the FBUnit interface. BORROWS the
 * module's live fb_fdm_state (the FDM the App owns and FBModule::Run() already advances every
 * substep) and derives Pose from it on read — no shadow copy of the truth to drift out of sync. Run()
 * is intentionally empty: the module cycles its own FDM/systems already (FBModule's own banner), so
 * there is nothing left for a per-unit tick to do for Ownship specifically; it exists purely so Ownship
 * satisfies the same FBUnit contract a sensor/weapon system queries every other unit through. */
#ifndef FBOWNSHIPUNIT_H
#define FBOWNSHIPUNIT_H

#include "FBUnit.h"
#include "jsbsim_adapter.h"

namespace FlightBox {

class FBOwnshipUnit : public FBUnit {
public:
  /* `state` is the App-owned fb_fdm_state the module steps each frame; its lifetime spans the whole
   * run, so a plain reference (no ownership) is safe to hold. */
  FBOwnshipUnit(int id, const fb_fdm_state &state)
      : FBUnit(id, FBUnitKind::Aircraft, FBUnitTeam::Friendly), State(state) {}

  FBUnitPose GetPose() const override;

private:
  const fb_fdm_state &State;
};

} // namespace FlightBox
#endif
