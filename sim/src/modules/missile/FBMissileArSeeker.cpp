#include "FBMissileArSeeker.h"

namespace FlightBox::Modules {

FBMissileArSeeker::FBMissileArSeeker() {
  /* A round has no scope to declutter, and the SEARCH sweep of a site is exactly what it is sent
   * against: hiding search symbols would make an early-warning shot impossible. */
  SetSearchShown(true);
  /* NOT powered here: a round whose catalogue entry names a different seeker must not publish a
   * receiver block at all, or its trace would claim a detector it does not carry. The guidance powers
   * this head, and only for an FBSeekerKind::AntiRadiation round. */
  SetPowered(false);
}

} // namespace FlightBox::Modules
